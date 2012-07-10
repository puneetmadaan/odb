// file      : odb/relational/mssql/context.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>
#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/relational/mssql/context.hxx>

using namespace std;

namespace relational
{
  namespace mssql
  {
    namespace
    {
      struct type_map_entry
      {
        const char* const cxx_type;
        const char* const db_type;
        const char* const db_id_type;
      };

      type_map_entry type_map[] =
      {
        {"bool", "BIT", 0},

        {"char", "TINYINT", 0},
        {"signed char", "TINYINT", 0},
        {"unsigned char", "TINYINT", 0},

        {"short int", "SMALLINT", 0},
        {"short unsigned int", "SMALLINT", 0},

        {"int", "INT", 0},
        {"unsigned int", "INT", 0},

        {"long int", "BIGINT", 0},
        {"long unsigned int", "BIGINT", 0},

        {"long long int", "BIGINT", 0},
        {"long long unsigned int", "BIGINT", 0},

        {"float", "REAL", 0},
        {"double", "FLOAT", 0},

        {"::std::string", "VARCHAR(512)", "VARCHAR(256)"},
        {"::std::wstring", "NVARCHAR(512)", "NVARCHAR(256)"},

        {"::size_t", "BIGINT", 0},
        {"::std::size_t", "BIGINT", 0},

        // Windows GUID/UUID (typedef struct _GUID {...} GUID, UUID;).
        //
        {"::_GUID", "UNIQUEIDENTIFIER", 0}
      };
    }

    context* context::current_;

    context::
    ~context ()
    {
      if (current_ == this)
        current_ = 0;
    }

    context::
    context (ostream& os,
             semantics::unit& u,
             options_type const& ops,
             features_type& f,
             sema_rel::model* m)
        : root_context (os, u, ops, f, data_ptr (new (shared) data (os))),
          base_context (static_cast<data*> (root_context::data_.get ()), m),
          data_ (static_cast<data*> (base_context::data_))
    {
      assert (current_ == 0);
      current_ = this;

      generate_grow = false;
      need_alias_as = true;
      insert_send_auto_id = false;
      delay_freeing_statement_result = true;
      need_image_clone = true;
      data_->bind_vector_ = "mssql::bind*";

      // Populate the C++ type to DB type map.
      //
      for (size_t i (0); i < sizeof (type_map) / sizeof (type_map_entry); ++i)
      {
        type_map_entry const& e (type_map[i]);

        type_map_type::value_type v (
          e.cxx_type,
          db_type_type (e.db_type, e.db_id_type ? e.db_id_type : e.db_type));

        data_->type_map_.insert (v);
      }
    }

    context::
    context ()
        : data_ (current ().data_)
    {
    }

    string const& context::
    convert_expr (string const& sqlt, semantics::data_member& m, bool to)
    {
      sql_type const& t (parse_sql_type (sqlt, m));
      return to ? t.to : t.from;
    }

    string context::
    quote_id_impl (qname const& id) const
    {
      string r;

      bool f (true);
      for (qname::iterator i (id.begin ()); i < id.end (); ++i)
      {
        if (i->empty ())
          continue;

        if (f)
          f = false;
        else
          r += '.';

        r += '[';
        r.append (*i, 0, 128); // Max identifier length is 128.
        r += ']';
      }

      return r;
    }

    string context::
    database_type_impl (semantics::type& t, semantics::names* hint, bool id)
    {
      string r (base_context::database_type_impl (t, hint, id));

      if (!r.empty ())
        return r;

      using semantics::enum_;

      if (t.is_a<semantics::enum_> ())
        r = "INT";

      return r;
    }

    //
    // SQL type parsing.
    //

    namespace
    {
      struct sql_parser
      {
        typedef context::invalid_sql_type invalid_sql_type;

        sql_parser (custom_db_types const* ct): ct_ (ct) {}

        sql_type
        parse (std::string sql)
        {
          r_ = sql_type ();
          m_.clear ();

          // First run the type through the custom mapping, if requested.
          //
          if (ct_ != 0)
          {
            for (custom_db_types::const_iterator i (ct_->begin ());
                 i != ct_->end (); ++i)
            {
              custom_db_type const& t (*i);

              if (t.type.match (sql))
              {
                r_.to = t.type.replace (sql, t.to);
                r_.from = t.type.replace (sql, t.from);
                sql = t.type.replace (sql, t.as);
                break;
              }
            }
          }

          l_.lex (sql);

          bool ok (true);

          try
          {
            ok = parse_name ();
          }
          catch (sql_lexer::invalid_input const& e)
          {
            ok = false;
            m_ = "invalid SQL Server type declaration: " + e.message;
          }

          if (!ok)
          {
            if (ct_ == 0)
              return sql_type ();
            else
              throw invalid_sql_type (m_);
          }

          return r_;
        }

        bool
        parse_name ()
        {
          sql_token t (l_.next ());

          if (t.type () != sql_token::t_identifier)
          {
            m_ = "expected SQL Server type name instead of '" +
              t.string () + "'";
            return false;
          }

          string id (upcase (t.identifier ()));

          if (id == "BIT")
          {
            r_.type = sql_type::BIT;
          }
          else if (id == "TINYINT")
          {
            r_.type = sql_type::TINYINT;
          }
          else if (id == "SMALLINT")
          {
            r_.type = sql_type::SMALLINT;
          }
          else if (id == "INT" ||
                   id == "INTEGER")
          {
            r_.type = sql_type::INT;
          }
          else if (id == "BIGINT")
          {
            r_.type = sql_type::BIGINT;
          }
          else if (id == "DECIMAL" ||
                   id == "NUMERIC" ||
                   id == "DEC")
          {
            r_.type = sql_type::DECIMAL;

            r_.has_prec = true;
            r_.prec = 18;

            r_.has_scale = true;
            r_.scale = 0;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "SMALLMONEY")
          {
            r_.type = sql_type::SMALLMONEY;
          }
          else if (id == "MONEY")
          {
            r_.type = sql_type::MONEY;
          }
          else if (id == "REAL")
          {
            r_.type = sql_type::FLOAT;

            r_.has_prec = true;
            r_.prec = 24;
          }
          else if (id == "FLOAT")
          {
            r_.type = sql_type::FLOAT;

            r_.has_prec = true;
            r_.prec = 53;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "DOUBLE")
          {
            t = l_.next ();

            if (t.type () != sql_token::t_identifier ||
                upcase (t.identifier ()) != "PRECISION")
            {
              m_ = "expected 'PRECISION' instead of '" + t.string () + "'";
              return false;
            }

            r_.type = sql_type::FLOAT;

            r_.has_prec = true;
            r_.prec = 53;

            // It appears that DOUBLE PRECISION can be followed by the
            // precision specification.
            //
            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "CHAR" ||
                   id == "CHARACTER")
          {
            if (!parse_char_trailer (false))
              return false;
          }
          else if (id == "VARCHAR")
          {
            r_.type = sql_type::VARCHAR;

            r_.has_prec = true;
            r_.prec = 1;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "TEXT")
          {
            r_.type = sql_type::TEXT;
          }
          else if (id == "NCHAR")
          {
            r_.type = sql_type::NCHAR;

            r_.has_prec = true;
            r_.prec = 1;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "NVARCHAR")
          {
            r_.type = sql_type::NVARCHAR;

            r_.has_prec = true;
            r_.prec = 1;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "NTEXT")
          {
            r_.type = sql_type::NTEXT;
          }
          else if (id == "NATIONAL")
          {
            t = l_.next ();

            if (t.type () == sql_token::t_identifier)
              id = upcase (t.identifier ());

            if (id == "TEXT")
            {
              r_.type = sql_type::NTEXT;
            }
            else if (id == "CHAR" ||
                     id == "CHARACTER")
            {
              if (!parse_char_trailer (true))
                return false;
            }
            else
            {
              m_ = "expected 'CHAR', 'CHARACTER', or 'TEXT' instead of '"
                + t.string () + "'";
              return false;
            }
          }
          else if (id == "BINARY")
          {
            // Can be just BINARY or BINARY VARYING.
            //
            t = l_.next ();

            if (t.type () == sql_token::t_identifier)
              id = upcase (t.identifier ());

            if (id == "VARYING")
            {
              r_.type = sql_type::VARBINARY;
              t = l_.next ();
            }
            else
              r_.type = sql_type::BINARY;

            r_.has_prec = true;
            r_.prec = 1;

            if (!parse_precision (t))
              return false;
          }
          else if (id == "VARBINARY")
          {
            r_.type = sql_type::VARBINARY;

            r_.has_prec = true;
            r_.prec = 1;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "IMAGE")
          {
            r_.type = sql_type::IMAGE;
          }
          else if (id == "DATE")
          {
            r_.type = sql_type::DATE;
          }
          else if (id == "TIME")
          {
            r_.type = sql_type::TIME;

            r_.has_scale = true;
            r_.scale = 7;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "DATETIME")
          {
            r_.type = sql_type::DATETIME;
          }
          else if (id == "DATETIME2")
          {
            r_.type = sql_type::DATETIME2;

            r_.has_scale = true;
            r_.scale = 7;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "SMALLDATETIME")
          {
            r_.type = sql_type::SMALLDATETIME;
          }
          else if (id == "DATETIMEOFFSET")
          {
            r_.type = sql_type::DATETIMEOFFSET;

            r_.has_scale = true;
            r_.scale = 7;

            if (!parse_precision (l_.next ()))
              return false;
          }
          else if (id == "UNIQUEIDENTIFIER")
          {
            r_.type = sql_type::UNIQUEIDENTIFIER;
          }
          else if (id == "ROWVERSION" ||
                   id == "TIMESTAMP")
          {
            r_.type = sql_type::ROWVERSION;
          }
          else
          {
            m_ = "unexpected SQL Server type name '" + t.identifier () + "'";
            return false;
          }

          return true;
        }

        bool
        parse_precision (sql_token t)
        {
          if (t.punctuation () == sql_token::p_lparen)
          {
            // Parse the precision.
            //
            t = l_.next ();

            if (t.type () == sql_token::t_identifier &&
                upcase (t.identifier ()) == "MAX")
            {
              r_.prec = 0;
              r_.has_prec = true;
            }
            else if (t.type () == sql_token::t_int_lit)
            {
              unsigned short v;
              istringstream is (t.literal ());

              if (!(is >> v && is.eof ()))
              {
                m_ = "invalid precision value '" + t.literal () + "' in SQL "
                  "Server type declaration";
                return false;
              }

              switch (r_.type)
              {
              case sql_type::TIME:
              case sql_type::DATETIME2:
              case sql_type::DATETIMEOFFSET:
                {
                  r_.scale = v;
                  r_.has_scale = true;
                  break;
                }
              default:
                {
                  r_.prec = v;
                  r_.has_prec = true;
                  break;
                }
              }
            }
            else
            {
              m_ = "integer precision expected in SQL Server type declaration";
              return false;
            }

            // Parse the scale if present.
            //
            t = l_.next ();

            if (t.punctuation () == sql_token::p_comma)
            {
              // Scale can only be specified for the DECIMAL type.
              //
              if (r_.type != sql_type::DECIMAL)
              {
                m_ = "unexpected scale in SQL Server type declaration";
                return false;
              }

              t = l_.next ();

              if (t.type () != sql_token::t_int_lit)
              {
                m_ = "integer scale expected in SQL Server type declaration";
                return false;
              }

              istringstream is (t.literal ());

              if (!(is >> r_.scale && is.eof ()))
              {
                m_ = "invalid scale value '" + t.literal () + "' in SQL "
                  "Server type declaration";
                return false;
              }

              r_.has_scale = true;
              t = l_.next ();
            }

            if (t.punctuation () != sql_token::p_rparen)
            {
              m_ = "expected ')' in SQL Server type declaration";
              return false;
            }
          }

          return true;
        }

        bool
        parse_char_trailer (bool nat)
        {
          sql_token t (l_.next ());

          string id;

          if (t.type () == sql_token::t_identifier)
            id = upcase (t.identifier ());

          if (id == "VARYING")
          {
            r_.type = nat ? sql_type::NVARCHAR : sql_type::VARCHAR;
            t = l_.next ();
          }
          else
            r_.type = nat ? sql_type::NCHAR : sql_type::CHAR;

          r_.has_prec = true;
          r_.prec = 1;

          return parse_precision (t);
        }

      private:
        string
        upcase (string const& s)
        {
          return context::upcase (s);
        }

      private:
        custom_db_types const* ct_;
        sql_lexer l_;
        sql_type r_;
        string m_; // Error message.
      };
    }

    sql_type const& context::
    parse_sql_type (string const& t, semantics::data_member& m, bool custom)
    {
      // If this proves to be too expensive, we can maintain a cache of
      // parsed types across contexts.
      //
      data::sql_type_cache::iterator i (data_->sql_type_cache_.find (t));

      if (i != data_->sql_type_cache_.end ()
          && (custom ? i->second.custom_cached : i->second.straight_cached))
      {
        return (custom ? i->second.custom : i->second.straight);
      }
      else
      {
        try
        {
          sql_type st (
            parse_sql_type (
              t,
              custom ? &unit.get<custom_db_types> ("custom-db-types") : 0));

          if (custom)
            return data_->sql_type_cache_[t].cache_custom (st);
          else
            return data_->sql_type_cache_[t].cache_straight (st);
        }
        catch (invalid_sql_type const& e)
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: " << e.message () << endl;

          throw operation_failed ();
        }
      }
    }

    sql_type context::
    parse_sql_type (string const& sqlt, custom_db_types const* ct)
    {
      sql_parser p (ct);
      return p.parse (sqlt);
    }
  }
}
