// file      : odb/generator.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cctype>  // std::toupper, std::is{alpha,upper,lower}
#include <string>
#include <memory>  // std::auto_ptr
#include <fstream>
#include <iostream>

#include <cutl/fs/auto-remove.hxx>

#include <cutl/compiler/code-stream.hxx>
#include <cutl/compiler/cxx-indenter.hxx>
#include <cutl/compiler/sloc-counter.hxx>

#include <odb/version.hxx>
#include <odb/context.hxx>
#include <odb/generator.hxx>

#include <odb/generate.hxx>
#include <odb/relational/generate.hxx>

using namespace std;
using namespace cutl;

using semantics::path;
typedef vector<string> strings;
typedef vector<path> paths;

namespace
{
  static char const cxx_file_header[] =
    "// This file was generated by ODB, object-relational mapping (ORM)\n"
    "// compiler for C++.\n"
    "//\n\n";

  static char const sql_file_header[] =
    "/* This file was generated by ODB, object-relational mapping (ORM)\n"
    " * compiler for C++.\n"
    " */\n\n";

  void
  open (ifstream& ifs, string const& path)
  {
    ifs.open (path.c_str (), ios_base::in | ios_base::binary);

    if (!ifs.is_open ())
    {
      cerr << path << ": error: unable to open in read mode" << endl;
      throw generator::failed ();
    }
  }

  void
  append (ostream& os, strings const& text)
  {
    for (strings::const_iterator i (text.begin ());
         i != text.end (); ++i)
    {
      os << *i << endl;
    }
  }

  void
  append (ostream& os, string const& file)
  {
    ifstream ifs;
    open (ifs, file);
    os << ifs.rdbuf ();
  }
}

void generator::
generate (options const& ops,
          features& fts,
          semantics::unit& unit,
          path const& p,
          paths const& inputs)
{
  try
  {
    database db (ops.database ()[0]);
    multi_database md (ops.multi_database ());

    // First create the database model.
    //
    cutl::shared_ptr<semantics::relational::model> model;

    if (ops.generate_schema ())
    {
      auto_ptr<context> ctx (create_context (cerr, unit, ops, fts, 0));

      switch (db)
      {
      case database::common:
        {
          break; // No schema for common.
        }
      case database::mssql:
      case database::mysql:
      case database::oracle:
      case database::pgsql:
      case database::sqlite:
        {
          model = relational::model::generate ();
          break;
        }
      }
    }

    // Output files.
    //
    path file (ops.output_name ().empty ()
               ? p.leaf ()
               : path (ops.output_name ()).leaf ());
    string base (file.base ().string ());

    fs::auto_removes auto_rm;

    string hxx_name (base + ops.odb_file_suffix ()[db] + ops.hxx_suffix ());
    string ixx_name (base + ops.odb_file_suffix ()[db] + ops.ixx_suffix ());
    string cxx_name (base + ops.odb_file_suffix ()[db] + ops.cxx_suffix ());
    string sch_name (base + ops.schema_file_suffix ()[db] + ops.cxx_suffix ());
    string sql_name (base + ops.sql_file_suffix ()[db] + ops.sql_suffix ());

    path hxx_path (hxx_name);
    path ixx_path (ixx_name);
    path cxx_path (cxx_name);
    path sch_path (sch_name);
    path sql_path (sql_name);

    if (!ops.output_dir ().empty ())
    {
      path dir (ops.output_dir ());
      hxx_path = dir / hxx_path;
      ixx_path = dir / ixx_path;
      cxx_path = dir / cxx_path;
      sch_path = dir / sch_path;
      sql_path = dir / sql_path;
    }

    bool gen_cxx (!ops.generate_schema_only ());

    //
    //
    ofstream hxx;

    if (gen_cxx)
    {
      hxx.open (hxx_path.string ().c_str (), ios_base::out);

      if (!hxx.is_open ())
      {
        cerr << "error: unable to open '" << hxx_path << "' in write mode"
             << endl;
        throw failed ();
      }

      auto_rm.add (hxx_path);
    }

    //
    //
    ofstream ixx;

    if (gen_cxx)
    {
      ixx.open (ixx_path.string ().c_str (), ios_base::out);

      if (!ixx.is_open ())
      {
        cerr << "error: unable to open '" << ixx_path << "' in write mode"
             << endl;
        throw failed ();
      }

      auto_rm.add (ixx_path);
    }

    //
    //
    ofstream cxx;

    if (gen_cxx && (db != database::common || md == multi_database::dynamic))
    {
      cxx.open (cxx_path.string ().c_str (), ios_base::out);

      if (!cxx.is_open ())
      {
        cerr << "error: unable to open '" << cxx_path << "' in write mode"
             << endl;
        throw failed ();
      }

      auto_rm.add (cxx_path);
    }

    //
    //
    bool gen_sql_schema (ops.generate_schema () &&
                         ops.schema_format ()[db].count (schema_format::sql) &&
                         db != database::common);
    ofstream sql;

    if (gen_sql_schema)
    {
      sql.open (sql_path.string ().c_str (), ios_base::out);

      if (!sql.is_open ())
      {
        cerr << "error: unable to open '" << sql_path << "' in write mode"
             << endl;
        throw failed ();
      }

      auto_rm.add (sql_path);
    }

    //
    //
    bool gen_sep_schema (
      gen_cxx &&
      ops.generate_schema () &&
      ops.schema_format ()[db].count (schema_format::separate) &&
      db != database::common);

    ofstream sch;

    if (gen_sep_schema)
    {
      sch.open (sch_path.string ().c_str (), ios_base::out);

      if (!sch.is_open ())
      {
        cerr << "error: unable to open '" << sch_path << "' in write mode"
             << endl;
        throw failed ();
      }

      auto_rm.add (sch_path);
    }

    // Print C++ headers.
    //
    if (gen_cxx)
    {
      hxx << cxx_file_header;
      ixx << cxx_file_header;

      if (db != database::common)
        cxx << cxx_file_header;
    }

    if (gen_sep_schema)
      sch << cxx_file_header;

    if (gen_sql_schema)
      sql << sql_file_header;

    typedef compiler::ostream_filter<compiler::cxx_indenter, char> ind_filter;
    typedef compiler::ostream_filter<compiler::sloc_counter, char> sloc_filter;

    size_t sloc_total (0);

    // Include settings.
    //
    string gp (ops.guard_prefix ());
    if (!gp.empty () && gp[gp.size () - 1] != '_')
      gp.append ("_");

    // HXX
    //
    if (gen_cxx)
    {
      auto_ptr<context> ctx (
        create_context (hxx, unit, ops, fts, model.get ()));

      sloc_filter sloc (ctx->os);

      string guard (ctx->make_guard (gp + hxx_name));

      hxx << "#ifndef " << guard << endl
          << "#define " << guard << endl
          << endl;

      // Version check.
      //
      hxx << "#include <odb/version.hxx>" << endl
          << endl
          << "#if (ODB_VERSION != " << ODB_VERSION << "UL)" << endl
          << "#error ODB runtime version mismatch" << endl
          << "#endif" << endl
          << endl;

      hxx << "#include <odb/pre.hxx>" << endl
          << endl;

      // Copy prologue.
      //
      {
        bool p (ops.hxx_prologue ().count (db) != 0);
        bool pf (ops.hxx_prologue_file ().count (db) != 0);

        if (p || pf)
        {
          hxx << "// Begin prologue." << endl
              << "//" << endl;
          if (p)
            append (hxx, ops.hxx_prologue ()[db]);
          if (pf)
            append (hxx, ops.hxx_prologue_file ()[db]);
          hxx << "//" << endl
              << "// End prologue." << endl
              << endl;
        }
      }

      // Include main file(s).
      //
      for (paths::const_iterator i (inputs.begin ()); i != inputs.end (); ++i)
        hxx << "#include " <<
          ctx->process_include_path (i->leaf ().string ()) << endl;
      hxx << endl;

      {
        // We don't want to indent prologues/epilogues.
        //
        ind_filter ind (ctx->os);

        // There are no -odb.hxx includes if we are generating code for
        // everything.
        //
        if (!ops.at_once ())
          include::generate (true);

        switch (db)
        {
        case database::common:
          {
            header::generate ();
            break;
          }
        case database::mssql:
        case database::mysql:
        case database::oracle:
        case database::pgsql:
        case database::sqlite:
          {
            if (md == multi_database::disabled)
              header::generate ();
            else
            {
              string n (base +
                        ops.odb_file_suffix ()[database::common] +
                        ops.hxx_suffix ());

              hxx << "#include " << ctx->process_include_path (n) << endl
                  << endl;
            }

            relational::header::generate ();
            break;
          }
        }
      }

      hxx << "#include " << ctx->process_include_path (ixx_name) << endl
          << endl;

      // Copy epilogue.
      //
      {
        bool e (ops.hxx_epilogue ().count (db) != 0);
        bool ef (ops.hxx_epilogue_file ().count (db) != 0);

        if (e || ef)
        {
          hxx << "// Begin epilogue." << endl
              << "//" << endl;
          if (e)
            append (hxx, ops.hxx_epilogue ()[db]);
          if (ef)
            append (hxx, ops.hxx_epilogue_file ()[db]);
          hxx << "//" << endl
              << "// End epilogue." << endl
              << endl;
        }
      }

      hxx << "#include <odb/post.hxx>" << endl
          << endl;

      hxx << "#endif // " << guard << endl;

      if (ops.show_sloc ())
        cerr << hxx_name << ": " << sloc.stream ().count () << endl;

      sloc_total += sloc.stream ().count ();
    }

    // IXX
    //
    if (gen_cxx)
    {
      auto_ptr<context> ctx (
        create_context (ixx, unit, ops, fts, model.get ()));

      sloc_filter sloc (ctx->os);

      // Copy prologue.
      //
      {
        bool p (ops.ixx_prologue ().count (db) != 0);
        bool pf (ops.ixx_prologue_file ().count (db) != 0);

        if (p || pf)
        {
          ixx << "// Begin prologue." << endl
              << "//" << endl;
          if (p)
            append (ixx, ops.ixx_prologue ()[db]);
          if (pf)
            append (ixx, ops.ixx_prologue_file ()[db]);
          ixx << "//" << endl
              << "// End prologue." << endl
              << endl;
        }
      }

      {
        // We don't want to indent prologues/epilogues.
        //
        ind_filter ind (ctx->os);

        switch (db)
        {
        case database::common:
          {
            inline_::generate ();
            break;
          }
        case database::mssql:
        case database::mysql:
        case database::oracle:
        case database::pgsql:
        case database::sqlite:
          {
            if (md == multi_database::disabled)
              inline_::generate ();

            relational::inline_::generate ();
            break;
          }
        }
      }

      // Copy epilogue.
      //
      {
        bool e (ops.ixx_epilogue ().count (db) != 0);
        bool ef (ops.ixx_epilogue_file ().count (db) != 0);

        if (e || ef)
        {
          ixx << "// Begin epilogue." << endl
              << "//" << endl;
          if (e)
            append (ixx, ops.ixx_epilogue ()[db]);
          if (ef)
            append (ixx, ops.ixx_epilogue_file ()[db]);
          ixx << "//" << endl
              << "// End epilogue." << endl
              << endl;
        }
      }

      if (ops.show_sloc ())
        cerr << ixx_name << ": " << sloc.stream ().count () << endl;

      sloc_total += sloc.stream ().count ();
    }

    // CXX
    //
    if (gen_cxx && (db != database::common || md == multi_database::dynamic))
    {
      auto_ptr<context> ctx (
        create_context (cxx, unit, ops, fts, model.get ()));

      sloc_filter sloc (ctx->os);

      cxx << "#include <odb/pre.hxx>" << endl
          << endl;

      // Copy prologue.
      //
      {
        bool p (ops.cxx_prologue ().count (db) != 0);
        bool pf (ops.cxx_prologue_file ().count (db) != 0);

        if (p || pf)
        {
          cxx << "// Begin prologue." << endl
              << "//" << endl;
          if (p)
            append (cxx, ops.cxx_prologue ()[db]);
          if (pf)
            append (cxx, ops.cxx_prologue_file ()[db]);
          cxx << "//" << endl
              << "// End prologue." << endl
              << endl;
        }
      }

      // Include query columns implementations for explicit instantiations.
      //
      string impl_guard;
      if (md == multi_database::dynamic && ops.extern_symbol ().empty ())
      {
        impl_guard = ctx->make_guard (
          "ODB_" + db.string () + "_QUERY_COLUMNS_DEF");

        cxx << "#define " << impl_guard << endl;
      }

      cxx << "#include " << ctx->process_include_path (hxx_name) << endl;

      if (!impl_guard.empty ())
        cxx << "#undef " << impl_guard << endl;

      cxx << endl;

      {
        // We don't want to indent prologues/epilogues.
        //
        ind_filter ind (ctx->os);

        // There are no -odb.hxx includes if we are generating code for
        // everything.
        //
        if (!ops.at_once ())
          include::generate (false);

        switch (db)
        {
        case database::common:
          {
            // Dynamic multi-database support.
            //
            source::generate ();
            break;
          }
        case database::mssql:
        case database::mysql:
        case database::oracle:
        case database::pgsql:
        case database::sqlite:
          {
            relational::source::generate ();
            break;
          }
        }
      }

      // Copy epilogue.
      //
      {
        bool e (ops.cxx_epilogue ().count (db) != 0);
        bool ef (ops.cxx_epilogue_file ().count (db) != 0);

        if (e || ef)
        {
          cxx << "// Begin epilogue." << endl
              << "//" << endl;
          if (e)
            append (cxx, ops.cxx_epilogue ()[db]);
          if (ef)
            append (cxx, ops.cxx_epilogue_file ()[db]);
          cxx << "//" << endl
              << "// End epilogue." << endl
              << endl;
        }
      }

      cxx << "#include <odb/post.hxx>" << endl;

      if (ops.show_sloc ())
        cerr << cxx_name << ": " << sloc.stream ().count () << endl;

      sloc_total += sloc.stream ().count ();
    }

    // SCH
    //
    if (gen_sep_schema)
    {
      auto_ptr<context> ctx (
        create_context (sch, unit, ops, fts, model.get ()));

      sloc_filter sloc (ctx->os);

      sch << "#include <odb/pre.hxx>" << endl
          << endl;

      // Copy prologue.
      //
      {
        bool p (ops.schema_prologue ().count (db) != 0);
        bool pf (ops.schema_prologue_file ().count (db) != 0);

        if (p || pf)
        {
          sch << "// Begin prologue." << endl
              << "//" << endl;
          if (p)
            append (sch, ops.schema_prologue ()[db]);
          if (pf)
            append (sch, ops.schema_prologue_file ()[db]);
          sch << "//" << endl
              << "// End prologue." << endl
              << endl;
        }
      }

      sch << "#include " << ctx->process_include_path (hxx_name) << endl
          << endl;

      {
        // We don't want to indent prologues/epilogues.
        //
        ind_filter ind (ctx->os);

        switch (db)
        {
        case database::common:
          {
            assert (false);
          }
        case database::mssql:
        case database::mysql:
        case database::oracle:
        case database::pgsql:
        case database::sqlite:
          {
            relational::schema_source::generate ();
            break;
          }
        }
      }

      // Copy epilogue.
      //
      {
        bool e (ops.schema_epilogue ().count (db) != 0);
        bool ef (ops.schema_epilogue_file ().count (db) != 0);

        if (e || ef)
        {
          sch << "// Begin epilogue." << endl
              << "//" << endl;
          if (e)
            append (sch, ops.schema_epilogue ()[db]);
          if (ef)
            append (sch, ops.schema_epilogue_file ()[db]);
          sch << "//" << endl
              << "// End epilogue." << endl
              << endl;
        }
      }

      sch << "#include <odb/post.hxx>" << endl;

      if (ops.show_sloc ())
        cerr << sch_name << ": " << sloc.stream ().count () << endl;

      sloc_total += sloc.stream ().count ();
    }

    // SQL
    //
    if (gen_sql_schema)
    {
      auto_ptr<context> ctx (
        create_context (sql, unit, ops, fts, model.get ()));

      switch (db)
      {
      case database::common:
        {
          assert (false);
        }
      case database::mssql:
      case database::mysql:
      case database::oracle:
      case database::pgsql:
      case database::sqlite:
        {
          relational::schema::generate_prologue ();

          // Copy prologue.
          //
          {
            bool p (ops.sql_prologue ().count (db) != 0);
            bool pf (ops.sql_prologue_file ().count (db) != 0);

            if (p || pf)
            {
              sql << "/* Begin prologue." << endl
                  << " */" << endl;
              if (p)
                append (sql, ops.sql_prologue ()[db]);
              if (pf)
                append (sql, ops.sql_prologue_file ()[db]);
              sql << "/*" << endl
                  << " * End prologue. */" << endl
                  << endl;
            }
          }

          if (!ops.omit_drop ())
            relational::schema::generate_drop ();

          // Copy interlude.
          //
          {
            bool i (ops.sql_interlude ().count (db) != 0);
            bool ifl (ops.sql_interlude_file ().count (db) != 0);

            if (i || ifl)
            {
              sql << "/* Begin interlude." << endl
                  << " */" << endl;
              if (i)
                append (sql, ops.sql_interlude ()[db]);
              if (ifl)
                append (sql, ops.sql_interlude_file ()[db]);
              sql << "/*" << endl
                  << " * End interlude. */" << endl
                  << endl;
            }
          }

          if (!ops.omit_create ())
            relational::schema::generate_create ();

          // Copy epilogue.
          //
          {
            bool e (ops.sql_epilogue ().count (db) != 0);
            bool ef (ops.sql_epilogue_file ().count (db) != 0);

            if (e || ef)
            {
              sql << "/* Begin epilogue." << endl
                  << " */" << endl;
              if (e)
                append (sql, ops.sql_epilogue ()[db]);
              if (ef)
                append (sql, ops.sql_epilogue_file ()[db]);
              sql << "/*" << endl
                  << " * End epilogue. */" << endl
                  << endl;
            }
          }

          relational::schema::generate_epilogue ();
          break;
        }
      }
    }

    // Communicate the sloc count to the driver. This is necessary to
    // correctly handle the total if we are compiling multiple files in
    // one invocation.
    //
    if (ops.show_sloc () || ops.sloc_limit_specified ())
      cout << "odb:sloc:" << sloc_total << endl;

    auto_rm.cancel ();
  }
  catch (operation_failed const&)
  {
    // Code generation failed. Diagnostics has already been issued.
    //
    throw failed ();
  }
  catch (semantics::invalid_path const& e)
  {
    cerr << "error: '" << e.path () << "' is not a valid filesystem path"
         << endl;
    throw failed ();
  }
  catch (fs::error const&)
  {
    // Auto-removal of generated files failed. Ignore it.
    //
    throw failed ();
  }
}
