// file      : odb/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_CONTEXT_HXX
#define ODB_CONTEXT_HXX

#include <map>
#include <set>
#include <stack>
#include <vector>
#include <string>
#include <ostream>
#include <cstddef> // std::size_t
#include <iostream>

#include <cutl/re.hxx>
#include <cutl/shared-ptr.hxx>

#include <odb/options.hxx>
#include <odb/semantics.hxx>
#include <odb/traversal.hxx>

using std::endl;
using std::cerr;

// Regex.
//
using cutl::re::regex;
using cutl::re::regexsub;

typedef std::vector<regexsub> regex_mapping;

//
//
class generation_failed {};

// Keep this enum synchronized with the one in libodb/odb/pointer-traits.hxx.
//
enum pointer_kind
{
  pk_raw,
  pk_unique,
  pk_shared,
  pk_weak
};

// Keep this enum synchronized with the one in libodb/odb/container-traits.hxx.
//
enum container_kind
{
  ck_ordered,
  ck_set,
  ck_multiset,
  ck_map,
  ck_multimap
};

class context
{
public:
  typedef std::size_t size_t;
  typedef std::string string;
  typedef std::vector<string> strings;
  typedef std::ostream ostream;

  typedef ::options options_type;

  static string
  upcase (string const&);

public:
  semantics::type&
  member_type (semantics::data_member& m, string const& key_prefix);

  // Predicates.
  //
public:

  // Check whether the type is a wrapper. Return the wrapped type if
  // it is a wrapper and NULL otherwise.
  //
  static semantics::type*
  wrapper (semantics::type& t)
  {
    return t.count ("wrapper") && t.get<bool> ("wrapper")
      ? t.get<semantics::type*> ("wrapper-type")
      : 0;
  }

  // Composite value type is a class type that was explicitly marked
  // as value type and there was no database type mapping provided for
  // it by the user (specifying the database type makes the value type
  // simple).
  //
  static bool
  comp_value (semantics::class_& c)
  {
    if (c.count ("composite-value"))
      return c.get<bool> ("composite-value");
    else
      return comp_value_ (c);
  }

  // Return the class object if this type is a composite value type
  // and NULL otherwise.
  //
  static semantics::class_*
  comp_value (semantics::type& t)
  {
    semantics::class_* c (dynamic_cast<semantics::class_*> (&t));
    return c != 0 && comp_value (*c) ? c : 0;
  }

  // As above but also "sees through" wrappers.
  //
  static semantics::class_*
  comp_value_wrapper (semantics::type& t)
  {
    if (semantics::class_* c = comp_value (t))
      return c;
    else if (semantics::type* wt = wrapper (t))
      return comp_value (*wt);
    else
      return 0;
  }

  static bool
  container (semantics::type& t)
  {
    return t.count ("container-kind");
  }

  static semantics::class_*
  object_pointer (semantics::type& t)
  {
    return t.get<semantics::class_*> ("element-type", 0);
  }

  static bool
  abstract (semantics::class_& c)
  {
    // If a class is abstract in the C++ sense then it is also abstract in
    // the database sense.
    //
    return c.abstract () || c.count ("abstract");
  }

  bool
  null (semantics::data_member&);

  bool
  null (semantics::data_member&, string const& key_prefix);

  // Database names and types.
  //
public:
  string
  table_name (semantics::class_&) const;

  // Table name for the container member. The table prefix passed as the
  // second argument must include the table prefix specified with the
  // --table-prefix option.
  //
  struct table_prefix
  {
    table_prefix (): level (0) {}
    table_prefix (string const& p, size_t l): prefix (p), level (l) {}

    string prefix;
    size_t level;
  };

  string
  table_name (semantics::data_member&, table_prefix const&) const;

  string
  column_name (semantics::data_member&) const;

  string
  column_name (semantics::data_member&,
               string const& key_prefix,
               string const& default_name) const;

  string
  column_type (semantics::data_member&, string const& key_prefix = string ());

  string
  column_options (semantics::data_member&);

  string
  column_options (semantics::data_member&, string const& key_prefix);

  // Cleaned-up member name that can be used for database names.
  //
  string
  public_name_db (semantics::data_member&) const;

  // C++ names.
  //
public:
  // Cleaned-up and escaped member name that can be used in public C++
  // interfaces.
  //
  string
  public_name (semantics::data_member&) const;

  // "Flatten" fully-qualified C++ name by replacing '::' with '_'
  // and removing leading '::', if any.
  //
  static string
  flat_name (string const& fqname);

  // Escape C++ keywords, reserved names, and illegal characters.
  //
  string
  escape (string const&) const;

  // Return a string literal that can be used in C++ source code. It
  // includes "".
  //
  string
  strlit (string const&);

  // Counts and other information.
  //
public:
  static size_t
  in_column_count (semantics::class_&);

  static size_t
  out_column_count (semantics::class_&);

  static semantics::data_member*
  id_member (semantics::class_& c)
  {
    // Set by the validator. May not be there for abstract objects.
    //
    return c.get<semantics::data_member*> ("id-member", 0);
  }

  // Object pointer information.
  //
public:
  typedef ::pointer_kind pointer_kind_type;

  pointer_kind_type
  pointer_kind (semantics::type& p)
  {
    return p.get<pointer_kind_type> ("pointer-kind");
  }

  bool
  lazy_pointer (semantics::type& p)
  {
    return p.get<bool> ("pointer-lazy");
  }

  bool
  weak_pointer (semantics::type& p)
  {
    return pointer_kind (p) == pk_weak;
  }

  semantics::data_member*
  inverse (semantics::data_member& m)
  {
    return object_pointer (m.type ())
      ? m.get<semantics::data_member*> ("inverse", 0)
      : 0;
  }

  semantics::data_member*
  inverse (semantics::data_member& m, string const& key_prefix)
  {
    if (key_prefix.empty ())
      return inverse (m);

    return object_pointer (member_type (m, key_prefix))
      ? m.get<semantics::data_member*> (key_prefix + "-inverse", 0)
      : 0;
  }

  // Container information.
  //
public:
  typedef ::container_kind container_kind_type;

  static container_kind_type
  container_kind (semantics::type& c)
  {
    return c.get<container_kind_type> ("container-kind");
  }

  static semantics::type&
  container_idt (semantics::type& c)
  {
    return *c.get<semantics::type*> ("id-tree-type");
  }

  static semantics::type&
  container_vt (semantics::type& c)
  {
    return *c.get<semantics::type*> ("value-tree-type");
  }

  static semantics::type&
  container_it (semantics::type& c)
  {
    return *c.get<semantics::type*> ("index-tree-type");
  }

  static semantics::type&
  container_kt (semantics::type& c)
  {
    return *c.get<semantics::type*> ("key-tree-type");
  }

  static bool
  unordered (semantics::data_member& m)
  {
    return m.count ("unordered") || m.type ().count ("unordered");
  }

  // The 'is a' and 'has a' tests. The has_a test currently does not
  // cross the container boundaries.
  //
public:
  static unsigned short const test_pointer = 0x01;
  static unsigned short const test_eager_pointer = 0x02;
  static unsigned short const test_lazy_pointer = 0x04;
  static unsigned short const test_container = 0x08;
  static unsigned short const test_straight_container = 0x10;
  static unsigned short const test_inverse_container = 0x20;

  bool
  is_a (semantics::data_member& m, unsigned short flags)
  {
    return is_a (m, flags, m.type (), "");
  }

  bool
  is_a (semantics::data_member&,
        unsigned short flags,
        semantics::type&,
        string const& key_prefix);

  bool
  has_a (semantics::type&, unsigned short flags);

public:
  // Process include path by adding the prefix, putting it through
  // the include regex list, and adding opening and closing include
  // characters ("" or <>) if necessary. The prefix argument indicates
  // whether the include prefix specified with the --include-prefix
  // option should be added. The open argument can be used to specify
  // the opening character. It can have three values: ", <, or \0. In
  // case of \0, the character is determined based on the value of the
  // --include-with-bracket option.
  //
  string
  process_include_path (string const&, bool prefix = true, char open = '\0');

  // Diverge output.
  //
public:
  void
  diverge (std::ostream& os)
  {
    diverge (os.rdbuf ());
  }

  void
  diverge (std::streambuf* sb);

  void
  restore ();

  // Implementation details.
  //
private:
  static bool
  comp_value_ (semantics::class_&);

  template <typename X>
  X
  indirect_value (semantics::context const& c, string const& key)
  {
    typedef X (*func) ();
    std::type_info const& ti (c.type_info (key));

    if (ti == typeid (func))
      return c.get<func> (key) ();
    else
      return c.get<X> (key);
  }

public:
  typedef std::set<string> keyword_set_type;

  struct db_type_type
  {
    db_type_type () {}
    db_type_type (string const& t, string const& it)
        : type (t), id_type (it)
    {
    }

    string type;
    string id_type;
  };
  typedef std::map<string, db_type_type> type_map_type;

protected:
  struct data
  {
    virtual
    ~data () {}
    data (std::ostream& os)
        : os_ (os.rdbuf ()), top_object_ (0), object_ (0)
    {
    }

  public:
    std::ostream os_;
    std::stack<std::streambuf*> os_stack_;

    semantics::class_* top_object_;
    semantics::class_* object_;

    keyword_set_type keyword_set_;
    type_map_type type_map_;

    regex_mapping include_regex_;
  };

  typedef cutl::shared_ptr<data> data_ptr;
  data_ptr data_;

public:
  std::ostream& os;
  semantics::unit& unit;
  options_type const& options;
  database const db;

  keyword_set_type const& keyword_set;

  regex_mapping const& include_regex;

  bool embedded_schema;

  // Outermost object currently being traversed.
  //
  semantics::class_*& top_object;

  // Object currently being traversed. It can be the same as top_object
  // or it can a base of top_object.
  //
  semantics::class_*& object;

  // Per-database customizable functionality.
  //
protected:
  // Return empty string if there is no mapping.
  //
  string
  database_type (semantics::type& t, semantics::names* hint, bool id)
  {
    return current ().database_type_impl (t, hint, id);
  }

  // The default implementation uses the type map (populated by the database-
  // specific context implementation) to come up with a mapping.
  //
  virtual string
  database_type_impl (semantics::type&, semantics::names*, bool);

public:
  typedef context root_context;

  virtual
  ~context ();
  context ();
  context (std::ostream&,
           semantics::unit&,
           options_type const&,
           data_ptr = data_ptr ());

  static context&
  current ()
  {
    return *current_;
  }

private:
  static context* current_;

private:
  context&
  operator= (context const&);
};

// Checks if scope Y names any of X.
//
template <typename X, typename Y>
bool
has (Y& y)
{
  for (semantics::scope::names_iterator i (y.names_begin ()),
         e (y.names_end ()); i != e; ++i)
    if (i->named (). template is_a<X> ())
      return true;

  return false;
}

// Standard namespace traverser.
//
struct namespace_: traversal::namespace_, context
{
  virtual void
  traverse (type&);
};

#endif // ODB_CONTEXT_HXX
