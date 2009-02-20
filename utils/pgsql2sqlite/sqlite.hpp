/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2009 Artem Pavlenko
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
//$Id$

// boost
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/variant.hpp>
//sqlite3
#include <sqlite3.h>

//stl
#ifdef MAPNIK_DEBUG
#include <iostream>         
#endif
#include <string>
#include <vector>

namespace mapnik {  namespace sqlite {

      class database : private boost::noncopyable
      {
         friend class prepared_statement;
         
         struct database_closer
         {
            void operator () (sqlite3 * db)
            {
#ifdef MAPNIK_DEBUG
               std::cerr << "close database " << db << "\n";
#endif
               sqlite3_close(db);
            }
         };
         
         typedef boost::shared_ptr<sqlite3> sqlite_db;    
         sqlite_db db_;
         
      public:
         database(std::string const& name);
         ~database();
         bool execute(std::string const& sql);
      };

      
      typedef boost::variant<int,double,std::string> value_type;
      typedef std::vector<value_type> record_type;
      
      class prepared_statement : boost::noncopyable 
      {
         struct binder : public boost::static_visitor<bool>
         {
            binder(sqlite3_stmt * stmt, unsigned index)
               : stmt_(stmt), index_(index) {}
            
            bool operator() (int val)
            {
               if (sqlite3_bind_int(stmt_, index_ , val ) != SQLITE_OK)
               {
                  std::cerr << "cannot bind " << val << "\n";
                  return false;
               }
               return true;
            }
            
            bool operator() (double val)
            {
               if (sqlite3_bind_double(stmt_, index_ , val ) != SQLITE_OK)
               {
                  std::cerr << "cannot bind " << val << "\n";
                  return false;
               }
               return true;
            }
            
            bool operator() (std::string const& val)
            {
               if (sqlite3_bind_text(stmt_, index_, val.c_str(), val.length(), 0) != SQLITE_OK)
               {
                 std::cerr << "cannot bind " << val << "\n";
                 return false;
               }
               return true;
            }
            
            sqlite3_stmt * stmt_;
            unsigned index_;
         };
      public:
         prepared_statement(database & db, std::string const& sql)
            : db_(db.db_.get()), stmt_(0)
         {
            const char * tail;
            char * err_msg;
            int res = sqlite3_prepare_v2(db_, sql.c_str(),-1, &stmt_,&tail);
            if (res != SQLITE_OK)
            {
               std::cerr << "ERR:"<< res << "\n";   
               throw;
            }
            
            // begin transaction
            res = sqlite3_exec(db_,"BEGIN;",0,0,&err_msg);
            if (res != SQLITE_OK)
            {
               std::cerr << "ERR:" << err_msg << "\n";     
               sqlite3_free(err_msg);       
            }
         }
         
         ~prepared_statement()
         {
            char * err_msg;
            //commit transaction
#ifdef MAPNIK_DEBUG
            std::cerr << "COMMIT\n";
#endif
            sqlite3_exec(db_,"COMMIT;",0,0,&err_msg);            
            int res = sqlite3_finalize(stmt_);
            if (res != SQLITE_OK)
            {
               std::cerr << "ERR:" << err_msg << "\n";     
               sqlite3_free(err_msg);       
            }
         }
         
         bool insert_record(record_type const& rec) const
         {  
            record_type::const_iterator itr = rec.begin();
            record_type::const_iterator end = rec.end();
            int count = 1;
            for (; itr!=end;++itr)
            {
               binder op(stmt_,count++);
               boost::apply_visitor(op,*itr);
            }

            sqlite3_step(stmt_);
            sqlite3_reset(stmt_);

            return true;
         }

      private:
         sqlite3 * db_;
         sqlite3_stmt * stmt_;
      };
      
      //
   }
}
