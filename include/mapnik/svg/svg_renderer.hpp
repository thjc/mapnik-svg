/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2010 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#ifndef MAPNIK_SVG_RENDERER_HPP
#define MAPNIK_SVG_RENDERER_HPP

#include <mapnik/svg/svg_path_attributes.hpp>
#include <mapnik/gradient.hpp>

#include <boost/utility.hpp>

#include "agg_path_storage.h"
#include "agg_conv_transform.h"
#include "agg_conv_stroke.h"
#include "agg_conv_contour.h"
#include "agg_conv_curve.h"
#include "agg_color_rgba.h"
#include "agg_renderer_scanline.h"
#include "agg_bounding_rect.h"
#include "agg_rasterizer_scanline_aa.h"


#include "agg_rendering_buffer.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_u.h"
#include "agg_scanline_p.h"
#include "agg_renderer_scanline.h"
#include "agg_span_allocator.h"
#include "agg_span_gradient.h"
#include "agg_gradient_lut.h"
#include "agg_gamma_lut.h"
#include "agg_span_interpolator_linear.h"
#include "agg_pixfmt_rgba.h"
#include "agg_path_storage.h"
#include "agg_ellipse.h"

#include <boost/foreach.hpp>

namespace mapnik  {
namespace svg {

template <typename VertexSource, typename AttributeSource> 
class svg_renderer : boost::noncopyable
{
    typedef agg::conv_curve<VertexSource>            curved_type;
    typedef agg::conv_stroke<curved_type>            curved_stroked_type;
    typedef agg::conv_transform<curved_stroked_type> curved_stroked_trans_type;    
    typedef agg::conv_transform<curved_type>         curved_trans_type;
    typedef agg::conv_contour<curved_trans_type>     curved_trans_contour_type;
    typedef agg::pixfmt_rgba32_plain pixfmt;
    typedef agg::renderer_base<pixfmt> renderer_base;
    typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;
    
public:
    svg_renderer(VertexSource & source, AttributeSource const& attributes)
        : source_(source),
          curved_(source_),
          curved_stroked_(curved_),
          attributes_(attributes) {}
    
    template <typename Rasterizer, typename Scanline, typename Renderer>
    void render(Rasterizer& ras, 
                Scanline& sl,
                Renderer& ren, 
                agg::trans_affine const& mtx, 
                double opacity=1.0)
    
    {
        using namespace agg;
        
        trans_affine transform;
        curved_stroked_trans_type curved_stroked_trans(curved_stroked_,transform);
        curved_trans_type         curved_trans(curved_,transform);
        curved_trans_contour_type curved_trans_contour(curved_trans);
        
        curved_trans_contour.auto_detect_orientation(true);
        
        for(unsigned i = 0; i < attributes_.size(); ++i)
        {
            mapnik::svg::path_attributes const& attr = attributes_[i];
            transform = attr.transform;
            transform *= mtx;
            double scl = transform.scale();
            //curved_.approximation_method(curve_inc);
            curved_.approximation_scale(scl);
            curved_.angle_tolerance(0.0);
            
            rgba8 color;
            
            if (attr.fill_flag)
            {
                ras.reset();
                
                if(fabs(curved_trans_contour.width()) < 0.0001)
                {
                    ras.add_path(curved_trans, attr.index);
                }
                else
                {
                    curved_trans_contour.miter_limit(attr.miter_limit);
                    ras.add_path(curved_trans_contour, attr.index);
                }

                if(true)
                {
                    typedef agg::gamma_lut<agg::int8u, agg::int8u> gamma_lut_type;
                    typedef agg::gradient_radial gradient_adaptor_type;
                    typedef agg::gradient_lut<agg::color_interpolator<agg::rgba8>, 1024> color_func_type;
                    typedef agg::span_interpolator_linear<> interpolator_type;
                    typedef agg::span_allocator<agg::rgba8> span_allocator_type;
                    typedef agg::span_gradient<agg::rgba8, 
                                               interpolator_type, 
                                               gradient_adaptor_type, 
                                               color_func_type> span_gradient_type;
                
                    span_allocator_type             m_alloc;
                    color_func_type                 m_gradient_lut;
                    gamma_lut_type                  m_gamma_lut;
                
                    m_gradient_lut.remove_all();
                    BOOST_FOREACH ( mapnik::stop_pair const& st, attr.gradient.get_stop_array() )
                    {
                        mapnik::color const& stop_color = st.second;
                        unsigned r= stop_color.red();
                        unsigned g= stop_color.green();
                        unsigned b= stop_color.blue();
                        unsigned a= stop_color.alpha();
                        //std::clog << "r: " << r << " g: " << g << " b: " << b << "a: " << a << "\n";
                        m_gradient_lut.add_color(st.first, agg::rgba8(r, g, b, int(a * attr.opacity * opacity)));
                    }
                    m_gradient_lut.build_lut();

                    double radius = 30;

                    gradient_adaptor_type gradient_adaptor;
                    
                    transform.invert();
                    interpolator_type     span_interpolator(transform);
                    span_gradient_type    span_gradient(span_interpolator, 
                                                      gradient_adaptor, 
                                                      m_gradient_lut, 
                                                      0, radius);
                                    
                    render_scanlines_aa(ras, sl, ren, m_alloc, span_gradient);
               
                }
                else
                {
                    ras.filling_rule(attr.even_odd_flag ? fill_even_odd : fill_non_zero);
                    color = attr.fill_color;
                    color.opacity(color.opacity() * attr.opacity * opacity);
                    renderer_solid ren_s(ren);
                    ren_s.color(color);
                    render_scanlines(ras, sl, ren_s);
                }
            }

            
            if(attr.stroke_flag)
            {
                std::clog << "stroking\n";
                curved_stroked_.width(attr.stroke_width);
                //m_curved_stroked.line_join((attr.line_join == miter_join) ? miter_join_round : attr.line_join);
                curved_stroked_.line_join(attr.line_join);
                curved_stroked_.line_cap(attr.line_cap);
                curved_stroked_.miter_limit(attr.miter_limit);
                curved_stroked_.inner_join(inner_round);
                curved_stroked_.approximation_scale(scl);

                // If the *visual* line width is considerable we 
                // turn on processing of curve cusps.
                //---------------------
                if(attr.stroke_width * scl > 1.0)
                {
                    curved_.angle_tolerance(0.2);
                }
                ras.reset();
                ras.filling_rule(fill_non_zero);
                ras.add_path(curved_stroked_trans, attr.index);
                color = attr.stroke_color;
                color.opacity(color.opacity() * attr.opacity * opacity);
                renderer_solid ren_s(ren);
                ren_s.color(color);
                render_scanlines(ras, sl, ren_s);
            }
        }
    }

private:
    
    VertexSource &  source_;
    curved_type          curved_;
    curved_stroked_type  curved_stroked_;
    AttributeSource const& attributes_;
};

}}

#endif //MAPNIK_SVG_RENDERER_HPP
