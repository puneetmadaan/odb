// file      : cli/generator.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cctype>  // std::toupper, std::is{alpha,upper,lower}
#include <string>
#include <memory>  // std::auto_ptr
#include <fstream>
#include <iostream>

#include <cutl/fs/auto-remove.hxx>

#include <cutl/compiler/code-stream.hxx>
#include <cutl/compiler/cxx-indenter.hxx>

#include <odb/version.hxx>
#include <odb/context.hxx>
#include <odb/generator.hxx>

#include <odb/generate.hxx>
#include <odb/tracer/generate.hxx>
#include <odb/relational/generate.hxx>
#include <odb/relational/type-processor.hxx>

#include <odb/relational/mysql/context.hxx>
#include <odb/relational/pgsql/context.hxx>
#include <odb/relational/sqlite/context.hxx>

using namespace std;
using namespace cutl;

using semantics::path;

namespace
{
  static char const file_header[] =
    "// This file was generated by ODB, object-relational mapping (ORM)\n"
    "// compiler for C++.\n"
    "//\n\n";

  string
  make_guard (string const& file, context& ctx)
  {
    string g (file);

    // Split words, e.g., "FooBar" to "Foo_Bar" and convert everything
    // to upper case.
    //
    string r;
    for (string::size_type i (0), n (g.size ()); i < n - 1; ++i)
    {
      char c1 (g[i]);
      char c2 (g[i + 1]);

      r += toupper (c1);

      if (isalpha (c1) && isalpha (c2) && islower (c1) && isupper (c2))
        r += "_";
    }
    r += toupper (g[g.size () - 1]);

    return ctx.escape (r);
  }

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
  append (ostream& os, vector<string> const& text, string const& file)
  {
    for (vector<string>::const_iterator i (text.begin ());
         i != text.end (); ++i)
    {
      os << *i << endl;
    }

    if (!file.empty ())
    {
      ifstream ifs;
      open (ifs, file);
      os << ifs.rdbuf ();
    }
  }
}

generator::
generator ()
{
}

static auto_ptr<context>
create_context (ostream& os, semantics::unit& unit, options const& ops)
{
  auto_ptr<context> r;

  switch (ops.database ())
  {
  case database::mysql:
    {
      r.reset (new relational::mysql::context (os, unit, ops));
      break;
    }
  case database::pgsql:
    {
      r.reset (new relational::pgsql::context (os, unit, ops));
      break;
    }
  case database::sqlite:
    {
      r.reset (new relational::sqlite::context (os, unit, ops));
      break;
    }
  case database::tracer:
    {
      r.reset (new context (os, unit, ops));
      break;
    }
  }

  return r;
}

void generator::
generate (options const& ops, semantics::unit& unit, path const& p)
{
  try
  {
    // Process types.
    //
    if (ops.database () != database::tracer)
    {
      auto_ptr<context> ctx (create_context (cerr, unit, ops));
      relational::process_types ();
    }

    // Output files.
    //
    path file (p.leaf ());
    string base (file.base ().string ());

    fs::auto_removes auto_rm;

    string hxx_name (base + ops.odb_file_suffix () + ops.hxx_suffix ());
    string ixx_name (base + ops.odb_file_suffix () + ops.ixx_suffix ());
    string cxx_name (base + ops.odb_file_suffix () + ops.cxx_suffix ());
    string sql_name (base + ops.sql_suffix ());

    path hxx_path (hxx_name);
    path ixx_path (ixx_name);
    path cxx_path (cxx_name);
    path sql_path (sql_name);

    if (!ops.output_dir ().empty ())
    {
      path dir (ops.output_dir ());
      hxx_path = dir / hxx_path;
      ixx_path = dir / ixx_path;
      cxx_path = dir / cxx_path;
      sql_path = dir / sql_path;
    }

    //
    //
    ofstream hxx (hxx_path.string ().c_str ());

    if (!hxx.is_open ())
    {
      cerr << "error: unable to open '" << hxx_path << "' in write mode"
           << endl;
      throw failed ();
    }

    auto_rm.add (hxx_path);

    //
    //
    ofstream ixx (ixx_path.string ().c_str ());

    if (!ixx.is_open ())
    {
      cerr << "error: unable to open '" << ixx_path << "' in write mode"
           << endl;
      throw failed ();
    }

    auto_rm.add (ixx_path);

    //
    //
    ofstream cxx (cxx_path.string ().c_str ());

    if (!cxx.is_open ())
    {
      cerr << "error: unable to open '" << cxx_path << "' in write mode"
           << endl;
      throw failed ();
    }

    auto_rm.add (cxx_path);

    //
    //
    bool sql_schema (ops.generate_schema () &&
                     ops.schema_format ().count (schema_format::sql));
    ofstream sql;

    if (sql_schema)
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

    // Print C++ headers.
    //
    hxx << file_header;
    ixx << file_header;
    cxx << file_header;

    typedef compiler::ostream_filter<compiler::cxx_indenter, char> cxx_filter;

    // Include settings.
    //
    string gp (ops.guard_prefix ());
    if (!gp.empty () && gp[gp.size () - 1] != '_')
      gp.append ("_");

    // HXX
    //
    {
      cxx_filter filt (hxx);
      auto_ptr<context> ctx (create_context (hxx, unit, ops));

      string guard (make_guard (gp + hxx_name, *ctx));

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
      hxx << "// Begin prologue." << endl
          << "//" << endl;
      append (hxx, ops.hxx_prologue (), ops.hxx_prologue_file ());
      hxx << "//" << endl
          << "// End prologue." << endl
          << endl;

      hxx << "#include " << ctx->process_include_path (file.string ()) << endl
          << endl;

      include::generate ();
      header::generate ();

      switch (ops.database ())
      {
      case database::mysql:
      case database::pgsql:
      case database::sqlite:
        {
          relational::header::generate ();
          break;
        }
      case database::tracer:
        {
          tracer::header::generate ();
          break;
        }
      }

      hxx << "#include " << ctx->process_include_path (ixx_name) << endl
          << endl;

      // Copy epilogue.
      //
      hxx << "// Begin epilogue." << endl
          << "//" << endl;
      append (hxx, ops.hxx_epilogue (), ops.hxx_epilogue_file ());
      hxx << "//" << endl
          << "// End epilogue." << endl
          << endl;

      hxx << "#include <odb/post.hxx>" << endl
          << endl;

      hxx << "#endif // " << guard << endl;
    }

    // IXX
    //
    {
      cxx_filter filt (ixx);
      auto_ptr<context> ctx (create_context (ixx, unit, ops));

      // Copy prologue.
      //
      ixx << "// Begin prologue." << endl
          << "//" << endl;
      append (ixx, ops.ixx_prologue (), ops.ixx_prologue_file ());
      ixx << "//" << endl
          << "// End prologue." << endl
          << endl;

      inline_::generate ();

      switch (ops.database ())
      {
      case database::mysql:
      case database::pgsql:
      case database::sqlite:
        {
          relational::inline_::generate ();
          break;
        }
      case database::tracer:
        {
          tracer::inline_::generate ();
          break;
        }
      }

      // Copy epilogue.
      //
      ixx << "// Begin epilogue." << endl
          << "//" << endl;
      append (ixx, ops.ixx_epilogue (), ops.ixx_epilogue_file ());
      ixx << "//" << endl
          << "// End epilogue." << endl;
    }

    // CXX
    //
    {
      cxx_filter filt (cxx);
      auto_ptr<context> ctx (create_context (cxx, unit, ops));

      cxx << "#include <odb/pre.hxx>" << endl
          << endl;

      // Copy prologue.
      //
      cxx << "// Begin prologue." << endl
          << "//" << endl;
      append (cxx, ops.cxx_prologue (), ops.cxx_prologue_file ());
      cxx << "//" << endl
          << "// End prologue." << endl
          << endl;

      cxx << "#include " << ctx->process_include_path (hxx_name) << endl
          << endl;

      switch (ops.database ())
      {
      case database::mysql:
      case database::pgsql:
      case database::sqlite:
        {
          relational::source::generate ();
          break;
        }
      case database::tracer:
        {
          tracer::source::generate ();
          break;
        }
      }

      // Copy epilogue.
      //
      cxx << "// Begin epilogue." << endl
          << "//" << endl;
      append (cxx, ops.cxx_epilogue (), ops.cxx_epilogue_file ());
      cxx << "//" << endl
          << "// End epilogue." << endl
          << endl;

      cxx << "#include <odb/post.hxx>" << endl;
    }

    // SQL
    //
    if (sql_schema)
    {
      auto_ptr<context> ctx (create_context (sql, unit, ops));

      // Copy prologue.
      //
      append (sql, ops.sql_prologue (), ops.sql_prologue_file ());

      switch (ops.database ())
      {
      case database::mysql:
      case database::pgsql:
      case database::sqlite:
        {
          relational::schema::generate ();
          break;
        }
      case database::tracer:
        {
          cerr << "error: the tracer database does not have schema" << endl;
          throw failed ();
        }
      }

      // Copy epilogue.
      //
      append (sql, ops.sql_epilogue (), ops.sql_epilogue_file ());
    }

    auto_rm.cancel ();
  }
  catch (const generation_failed&)
  {
    // Code generation failed. Diagnostics has already been issued.
    //
    throw failed ();
  }
  catch (const re::format& e)
  {
    cerr << "error: invalid regex: '" << e.regex () << "': " <<
      e.description () << endl;
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
