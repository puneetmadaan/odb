// file      : odb/common-query.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common-query.hxx>

using namespace std;

// query_tags
//

void query_tags::
traverse (semantics::class_& c)
{
  if (object (c))
  {
    object_columns_base::traverse (c);
  }
  else if (c.get<size_t> ("object-count") != 0) // View.
  {
    view_objects& objs (c.get<view_objects> ("objects"));

    for (view_objects::const_iterator i (objs.begin ());
         i < objs.end ();
         ++i)
    {
      if (i->kind != view_object::object)
        continue; // Skip tables.

      if (i->alias.empty ())
        continue;

      generate (i->alias);
    }
  }

  if (nl_)
    os << endl;
}

void query_tags::
traverse_object (semantics::class_& c)
{
  names (c); // We don't want to traverse bases.
}

void query_tags::
traverse_composite (semantics::data_member* m, semantics::class_& c)
{
  // Base type.
  //
  if (m == 0)
  {
    object_columns_base::traverse_composite (m, c);
    return;
  }

  // Don't generate an empty struct if we don't have any pointers.
  //
  if (!has_a (c, test_pointer))
    return;

  if (nl_)
    os << endl;

  os << "struct " << public_name (*m) << "_tag"
     << "{";

  object_columns_base::traverse_composite (m, c);

  os << "};";

  nl_ = false;
}

void query_tags::
traverse_pointer (semantics::data_member& m, semantics::class_&)
{
  // Ignore polymorphic id references.
  //
  if (m.count ("polymorphic-ref"))
    return;

  generate (public_name (m));
}

void query_tags::
generate (string const& name)
{
  os << "struct " << name << "_tag;";
  nl_ = true;
}

// query_alias_traits
//

query_alias_traits::
query_alias_traits (semantics::class_& c, bool decl)
    : decl_ (decl)
{
  scope_ = "access::";
  scope_ += (object (c) ? "object_traits_impl" : "view_traits_impl");
  scope_ += "< " + class_fq_name (c) + ", id_" + db.string () + " >";
}

void query_alias_traits::
traverse_object (semantics::class_& c)
{
  // We don't want to traverse bases.
  //
  names (c);
}

void query_alias_traits::
traverse_composite (semantics::data_member* m, semantics::class_& c)
{
  // Base type.
  //
  if (m == 0)
  {
    object_columns_base::traverse_composite (m, c);
    return;
  }

  string old_scope (scope_);
  scope_ += "::" + public_name (*m) + "_tag";

  object_columns_base::traverse_composite (m, c);

  scope_ = old_scope;
}

void query_alias_traits::
traverse_pointer (semantics::data_member& m, semantics::class_& c)
{
  // Ignore polymorphic id references.
  //
  if (m.count ("polymorphic-ref"))
    return;

  if (decl_)
    generate_decl (public_name (m), c);
  else
    generate_def (m, c);
}

void query_alias_traits::
generate_decl (string const& tag, semantics::class_& c)
{
  semantics::class_* poly_root (polymorphic (c));
  bool poly_derived (poly_root != 0 && poly_root != &c);
  semantics::class_* poly_base (poly_derived ? &polymorphic_base (c) : 0);

  if (poly_derived)
    generate_decl (tag, *poly_base);

  string const& fq_name (class_fq_name (c));

  os << "template <>" << endl
     << "struct " << exp << "alias_traits<" << endl
     << "  " << fq_name << "," << endl
     << "  id_" << db << "," << endl
     << "  " << scope_ << "::" << tag << "_tag>"
     << "{";

  if (poly_derived)
    os << "typedef alias_traits<" << endl
       << "  " << class_fq_name (*poly_base) << "," << endl
       << "  id_" << db << "," << endl
       << "  " << scope_ << "::" << tag << "_tag>" << endl
       << "base_traits;"
       << endl;

  // For dynamic multi-database support also generate common traits
  // alias. Note that the tag type is the same since they all are
  // derived from object_traits.
  //
  if (db != database::common && multi_dynamic)
    os << "typedef alias_traits<" << endl
       << "  " << fq_name << "," << endl
       << "  id_common," << endl
       << "  " << scope_ << "::" << tag << "_tag>" << endl
       << "common_traits;"
       << endl;

  generate_decl_body (); // Table name, etc.

  os << "};";
}

void query_alias_traits::
generate_decl_body ()
{
}

void query_alias_traits::
generate_def (semantics::data_member&, semantics::class_&)
{
}

void query_alias_traits::
generate_def (string const&, semantics::class_&, string const&)
{
}

// query_columns_base
//

query_columns_base::
query_columns_base (semantics::class_& c, bool decl, bool inst)
    : decl_ (decl), inst_ (inst)
{
  string const& n (class_fq_name (c));

  if (decl)
    scope_ = "access::object_traits_impl< " + n + ", id_" +
      db.string () + " >";
  else
    scope_ = "query_columns_base< " + n + ", id_" + db.string () + " >";
}

void query_columns_base::
traverse_object (semantics::class_& c)
{
  // We don't want to traverse bases.
  //
  names (c);
}

void query_columns_base::
traverse_composite (semantics::data_member* m, semantics::class_& c)
{
  // Base type.
  //
  if (m == 0)
  {
    object_columns_base::traverse_composite (m, c);
    return;
  }

  // Don't generate an empty struct if we don't have any pointers.
  //
  if (!has_a (c, test_pointer))
    return;

  string name (public_name (*m));

  if (decl_)
  {
    os << "// " << name << endl
       << "//" << endl
       << "struct " << name << "_base_"
       << "{";

    string old_scope (scope_);
    scope_ += "::" + name + "_tag";

    object_columns_base::traverse_composite (m, c);

    scope_ = old_scope;

    os << "};";
  }
  else
  {
    string old_scope (scope_);
    scope_ += "::" + name + "_base_";

    object_columns_base::traverse_composite (m, c);

    scope_ = old_scope;
  }
}

void query_columns_base::
traverse_pointer (semantics::data_member& m, semantics::class_& c)
{
  // Ignore polymorphic id references.
  //
  if (m.count ("polymorphic-ref"))
    return;

  string name (public_name (m));
  string const& fq_name (class_fq_name (c));
  bool inv (inverse (m, key_prefix_));

  if (decl_)
  {
    os << "// " << name << endl
       << "//" << endl;

    os << "typedef" << endl
       << "odb::alias_traits<" << endl
       << "  " << fq_name << "," << endl
       << "  id_" << db << "," << endl
       << "  " << scope_ << "::" << name << "_tag>" << endl
       << name << "_alias_;"
       << endl;

    if (inv)
    {
      os << "typedef" << endl
         << "odb::query_pointer<" << endl
         << "  odb::pointer_query_columns<" << endl
         << "    " << fq_name << "," << endl
         << "    id_" << db << "," << endl
         << "    " << name << "_alias_ > >" << endl
         << name << "_type_ ;"
         << endl
         << "static " << const_ << name << "_type_ " << name << ";"
         << endl;
    }
  }
  else if (inst_)
  {
    generate_inst (m, c);
  }
  else
  {
    // Generate explicit template instantiation directive for the
    // pointed-to pointer_query_columns.
    //
    if (multi_dynamic)
      generate_inst (m, c);

    if (inv)
      os << const_ << scope_ << "::" << name << "_type_" << endl
         << scope_ << "::" << name << ";"
         << endl;
  }
}

void query_columns_base::
generate_inst (semantics::data_member& m, semantics::class_& c)
{
  string name (public_name (m));
  string const& fq_name (class_fq_name (c));

  // If the pointed-to class has no pointers of its own then
  // pointer_query_columns just derives from query_columns and
  // that's what we need to instantiate.
  //
  bool has_ptr (has_a (c, test_pointer | include_base));
  string alias (scope_ + "::" + name + "_alias_");

  // Instantiate base [pointer_]query_columns.
  //
  {
    instance<query_columns_base_insts> b (has_ptr, inst_, alias);
    traversal::inherits i (*b);
    inherits (c, i);
  }

  inst_header (inst_);
  os << (has_ptr ? "pointer_" : "") << "query_columns<" << endl
     << "  " << fq_name << "," << endl
     << "  id_" << db << "," << endl
     << "  " << alias << " >;"
     << endl;
}

// query_columns
//

query_columns::
query_columns (bool ptr)
    : ptr_ (ptr), decl_ (true), in_ptr_ (false)
{
}

query_columns::
query_columns (bool ptr, semantics::class_& c) //@@ context::{cur,top}_object
    : ptr_ (ptr), decl_ (false), in_ptr_ (false), fq_name_ (class_fq_name (c))
{
}

void query_columns::
traverse_object (semantics::class_& c)
{
  // We don't want to traverse bases.
  //
  names (c);
}

void query_columns::
traverse_composite (semantics::data_member* m, semantics::class_& c)
{
  // Base type.
  //
  if (m == 0)
  {
    object_columns_base::traverse_composite (m, c);
    return;
  }

  string name (public_name (*m));
  string suffix (in_ptr_ ? "_column_type_" : "_type_");

  if (decl_)
  {
    os << "// " << name << endl
       << "//" << endl
       << "struct " << name << suffix;

    // Derive from the base in query_columns_base. It contains columns
    // data for the pointer members.
    //
    if (!ptr_ && has_a (c, test_pointer))
      os << ": " << name << "_base_";

    os << "{";

    if (!const_.empty ())
      os << name << suffix << " ()" // Need user-defined default c-tor for
         << "{"                     // initialization of static const.
         << "}";

    object_columns_base::traverse_composite (m, c);

    os << "};";

    if (!in_ptr_)
      os << "static " << const_ << name << "_type_ " << name << ";"
         << endl;
  }
  else
  {
    // Handle nested members first.
    //
    string old_scope (scope_);
    scope_ += "::" + name + suffix;

    object_columns_base::traverse_composite (m, c);

    scope_ = old_scope;

    // Composite member. Note that here we don't use suffix.
    //
    string tmpl (ptr_ ? "pointer_query_columns" : "query_columns");
    tmpl += "< " + fq_name_ + ", id_" + db.string () + ", A >" + scope_;

    os << "template <typename A>" << endl
       << const_ << "typename " << tmpl << "::" << name << "_type_" << endl
       << tmpl << "::" << name << ";"
       << endl;
  }
}

void query_columns::
column_ctor (string const&, string const&, string const&)
{
}

void query_columns::
column_common (semantics::data_member& m,
               string const& type,
               string const&,
               string const& suffix)
{
  string name (public_name (m));

  if (decl_)
  {
    os << "// " << name << endl
       << "//" << endl;

    os << "typedef odb::query_column< " << type << " > " << name <<
      suffix << ";"
       << endl;
  }
  else
  {
    // Note that here we don't use suffix.
    //
    string tmpl (ptr_ ? "pointer_query_columns" : "query_columns");
    tmpl += "< " + fq_name_ + ", id_" + db.string () + ", A >" + scope_;

    os << "template <typename A>" << endl
       << const_ << "typename " << tmpl << "::" << name << "_type_" << endl
       << tmpl << "::" << name << ";"
       << endl;
  }
}

bool query_columns::
traverse_column (semantics::data_member& m, string const& column, bool)
{
  semantics::names* hint;
  semantics::type& t (utype (m, hint));

  column_common (m, t.fq_name (hint), column);

  if (decl_)
  {
    string name (public_name (m));

    os << "static " << const_ << name << "_type_ " << name << ";"
       << endl;
  }

  return true;
}

void query_columns::
traverse_pointer (semantics::data_member& m, semantics::class_& c)
{
  // Ignore polymorphic id references.
  //
  if (m.count ("polymorphic-ref"))
    return;

  // If this is for the pointer_query_columns and the member is not
  // inverse, then create the normal member corresponding to the id
  // column. This will allow the user to check it for NULL or to
  // compare ids. In case this is for query_columns, then for the
  // inverse member everything has been generated in query_columns_base.
  //
  if (inverse (m, key_prefix_))
    return;

  string name (public_name (m));

  semantics::data_member& id (*id_member (c));
  semantics::names* hint;
  semantics::type& t (utype (id, hint));

  if (composite_wrapper (t))
  {
    // Composite id.
    //

    // For pointer_query_columns generate normal composite mapping.
    //
    if (ptr_)
      object_columns_base::traverse_pointer (m, c);
    else
    {
      // If this is a non-inverse relationship, then make the column have
      // a dual interface: that of an object pointer and of an id column.
      // The latter allows the user to, for example, use the is_null()
      // test in a natural way. For inverse relationships there is no
      // column and so the column interface is not available.
      //
      in_ptr_ = true;
      object_columns_base::traverse_pointer (m, c);
      in_ptr_ = false;

      if (decl_)
      {
        os << "typedef" << endl
           << "odb::query_pointer<" << endl
           << "  odb::pointer_query_columns<" << endl
           << "    " << class_fq_name (c) << "," << endl
           << "    id_" << db << "," << endl
           << "    " << name << "_alias_ > >" << endl
           << name << "_pointer_type_;"
           << endl;

        os << "struct " << name << "_type_: " <<
          name << "_pointer_type_, " << name << "_column_type_"
           << "{";

        if (!const_.empty ())
          os << name << "_type_ ()" // Need user-defined default c-tor for
             << "{"                 // initialization of static const.
             << "}";

        os << "};";

        os << "static " << const_ << name << "_type_ " << name << ";"
           << endl;
      }
    }
  }
  else
  {
    // Simple id.
    //
    string type (t.fq_name (hint));
    string col (column_name (m, key_prefix_, default_name_, column_prefix_));

    // For pointer_query_columns generate normal column mapping.
    //
    if (ptr_)
      column_common (m, type, col);
    else
    {
      // If this is a non-inverse relationship, then make the column have
      // a dual interface: that of an object pointer and of an id column.
      // The latter allows the user to, for example, use the is_null()
      // test in a natural way. For inverse relationships there is no
      // column and so the column interface is not available.
      //
      column_common (m, type, col, "_column_type_");

      if (decl_)
      {
        os << "typedef" << endl
           << "odb::query_pointer<" << endl
           << "  odb::pointer_query_columns<" << endl
           << "    " << class_fq_name (c) << "," << endl
           << "    id_" << db << "," << endl
           << "    " << name << "_alias_ > >" << endl
           << name << "_pointer_type_;"
           << endl;

        os << "struct " << name << "_type_: " <<
          name << "_pointer_type_, " << name << "_column_type_"
           << "{";

        column_ctor (type, name + "_type_", name + "_column_type_");

        os << "};";
      }
    }

    if (decl_)
      os << "static " << const_ << name << "_type_ " << name << ";"
         << endl;
  }
}

// query_columns_bases
//

void query_columns_bases::
traverse (type& c)
{
  // Ignore transient bases. Not used for views.
  //
  if (!object (c))
    return;

  if (first_)
  {
    os << ":" << endl
       << "  ";
    first_ = false;
  }
  else
    os << "," << endl
       << "  ";

  os << (ptr_ ? "pointer_query_columns" : "query_columns") << "< " <<
    class_fq_name (c) << ", id_" << db << ", ";

  // If our base is polymorphic, then it has its own table/alias.
  //
  if (polymorphic (c))
    os << "typename A::base_traits";
  else
    os << "A";

  os << " >";
}

// query_columns_base_aliases
//

void query_columns_base_aliases::
traverse (type& c)
{
  // Ignore transient bases. Not used for views.
  //
  if (!object (c))
    return;

  string const& name (class_name (c));

  os << "// " << name << endl
     << "//" << endl
     << "typedef " << (ptr_ ? "pointer_query_columns" : "query_columns") <<
    "< " << class_fq_name (c) << ", id_" << db << ", ";

  if (polymorphic (c))
    os << "typename A::base_traits";
  else
    os << "A";

  os << " > " << name << ";"
     << endl;
}

// query_columns_base_insts
//

query_columns_base_insts::
query_columns_base_insts (bool ptr, bool decl, string const& alias)
    : ptr_ (ptr), decl_ (decl), alias_ (alias)
{
  *this >> inherits_ >> *this;
}

query_columns_base_insts::
query_columns_base_insts (query_columns_base_insts const& x)
    : context (), // @@ -Wextra
      ptr_ (x.ptr_), decl_ (x.decl_), alias_ (x.alias_)
{
  *this >> inherits_ >> *this;
}

void query_columns_base_insts::
traverse (type& c)
{
  // We are only interested in reuse inheritance.
  //
  if (!object (c) || polymorphic (c))
    return;

  // Instantiate bases recursively.
  //
  inherits (c, inherits_);

  inst_header (decl_);
  os << (ptr_ ? "pointer_query_columns" : "query_columns") << "<" << endl
     << "  " << class_fq_name (c) << "," << endl
     << "  id_" << db << "," << endl
     << "  " << alias_ << " >;"
     << endl;
}

// query_columns_type
//

void query_columns_type::
traverse (type& c)
{
  string const& type (class_fq_name (c));

  if (ptr_)
  {
    os << "template <typename A>" << endl
       << "struct pointer_query_columns< " << type << ", id_" << db << ", A >";

    // If we don't have pointers (in the whole hierarchy), then
    // pointer_query_columns and query_columns are the same.
    //
    if (!has_a (c, test_pointer | include_base))
    {
      os << ":" << endl
         << "  query_columns< " << type << ", id_" << db << ", A >"
         << "{"
         << "};";
    }
    else
    {
      {
        instance<query_columns_bases> b (ptr_);
        traversal::inherits i (*b);
        inherits (c, i);
      }

      os << "{";

      {
        instance<query_columns_base_aliases> b (ptr_);
        traversal::inherits i (*b);
        inherits (c, i);
      }

      {
        instance<query_columns> t (ptr_);
        t->traverse (c);
      }

      os << "};";

      generate_impl (c);
    }
  }
  else if (decl_)
  {
    bool has_ptr (has_a (c, test_pointer | exclude_base));

    if (has_ptr)
    {
      // Generate aliases.
      //
      {
        bool true_ (true); //@@ (im)perfect forwarding
        instance<query_alias_traits> t (c, true_);
        t->traverse (c);
      }

      // This class contains everything for inverse pointers and
      // aliases for non-inverse ones. It doesn't depend on the
      // table alias (A) template argument.
      //
      os << "template <>" << endl
         << "struct " << exp << "query_columns_base< " << type << ", " <<
        "id_" << db << " >"
         << "{";

      bool true_ (true); //@@ (im)perfect forwarding.
      bool false_ (false);
      instance<query_columns_base> t (c, true_, false_);
      t->traverse (c);

      os << "};";
    }

    os << "template <typename A>" << endl
       << "struct query_columns< " << type << ", id_" << db << ", A >";

    if (has_ptr)
      os << ":" << endl
         << "  query_columns_base< " << type << ", id_" << db << " >";

    {
      instance<query_columns_bases> b (ptr_, !has_ptr);
      traversal::inherits i (*b);
      inherits (c, i);
    }

    os << "{";

    {
      instance<query_columns_base_aliases> b (ptr_);
      traversal::inherits i (*b);
      inherits (c, i);
    }

    {
      instance<query_columns> t (ptr_);
      t->traverse (c);
    }

    os << "};";

    generate_impl (c);
  }
  else if (inst_)
  {
    // If we have the extern symbol, generate extern template declarations.
    //
    if (!ext.empty ())
    {
      os << "#ifdef " << ext << endl
         << endl;

      if (has_a (c, test_pointer | exclude_base))
      {
        bool true_ (true); //@@ (im)perfect forwarding.
        bool false_ (false);

        instance<query_columns_base> t (c, false_, true_);
        t->traverse (c);
      }

      generate_inst (c);

      os << "#endif // " << ext << endl
         << endl;
    }
  }
  else
  {
    bool has_ptr (has_a (c, test_pointer | exclude_base));

    // Generate alias_traits specializations. While the class
    // is generated even if our base has a pointer, there is
    // not source code if we don't have pointers ourselves.
    //
    if (has_ptr)
    {
      bool false_ (false); //@@ (im)perfect forwarding
      instance<query_alias_traits> t (c, false_);
      t->traverse (c);
    }

    // query_columns_base
    //
    if (has_ptr)
    {
      bool false_ (false); //@@ (im)perfect forwarding.
      instance<query_columns_base> t (c, false_, false_);
      t->traverse (c);
    }

    // Explicit template instantiations.
    //
    if (multi_dynamic)
      generate_inst (c);
  }
}

void query_columns_type::
generate_impl (type& c)
{
  string guard;

  // Exclude definitions (they will be explicitly instantiated once in
  // the source file) unless we have the extern symbol. In this case
  // the extern template declaration will make sure we don't get
  // instantiations in multiple places and we will avoid the VC++
  // warning C4661 (no definition provided).
  //
  if (multi_dynamic && ext.empty ())
  {
    guard = make_guard ("ODB_" + db.string () + "_QUERY_COLUMNS_DEF");

    os << "#ifdef " << guard << endl
       << endl;
  }

  instance<query_columns> t (ptr_, c);
  t->traverse (c);

  if (!guard.empty ())
    os << "#endif // " << guard << endl
       << endl;
}

void query_columns_type::
generate_inst (type& c)
{
  string const& type (class_fq_name (c));

  // Explicit template instantiations. Here is what we need to
  // instantiate
  //
  // 1. Reuse inheritance bases all the way to the ultimate base.
  //    Unlike poly inheritance, reuse inheritance uses the table
  //    alias of the derived type. Note that bases can have object
  //    pointers of their own but their types have already been
  //    instantiated by step 3 below performed for the base object.
  //
  // 2. Object pointers. Note that while object pointers cannot have
  //    their own pointers, they can have reuse inheritance bases.
  //
  // 3. The query_columns class for the table itself.
  //
  string alias ("access::object_traits_impl< " + type + ", id_" +
                db.string () + " >");

  // 1
  //
  {
    instance<query_columns_base_insts> b (false, inst_, alias);
    traversal::inherits i (*b);
    inherits (c, i);
  }

  // 2: Handled by query_columns_base (which is where we generate
  //    all the aliases for object pointers).
  //

  // 3
  //
  inst_header (inst_);
  os << "query_columns<" << endl
     << "  " << type << "," << endl
     << "  id_" << db << "," << endl
     << "  " << alias << " >;"
     << endl;
}

// view_query_columns_type
//

void view_query_columns_type::
traverse (type& c)
{
  if (decl_)
    generate_decl (c);
  else
    generate_def (c);
}

void view_query_columns_type::
generate_decl (type& c)
{
  string const& type (class_fq_name (c));
  size_t obj_count (c.get<size_t> ("object-count"));
  view_objects& objs (c.get<view_objects> ("objects"));

  // Generate alias_traits specializations.
  //
  {
    bool true_ (true); //@@ (im)perfect forwarding
    instance<query_alias_traits> at (c, true_);

    for (view_objects::const_iterator i (objs.begin ()); i < objs.end (); ++i)
    {
      if (i->kind != view_object::object)
        continue; // Skip tables.

      if (i->alias.empty ())
        continue;

      semantics::class_& o (*i->obj);
      qname const& t (table_name (o));

      // Check that the alias is not the same as the table name
      // (if this is a polymorphic object, then the alias is just
      // a prefix).
      //
      if (polymorphic (o) || t.qualified () || i->alias != t.uname ())
        at->generate_decl (i->alias, o);
    }
  }

  // If we have the extern symbol, generate extern template declarations.
  // Do it before query_columns since the inheritance will trigger
  // instantiation and we won't be able to change visibility (GCC).
  //
  if (obj_count != 0 && multi_dynamic && !ext.empty ())
  {
    os << "#ifdef " << ext << endl
       << endl;

    generate_inst (c);

    os << "#endif // " << ext << endl
       << endl;
  }

  // query_columns
  //
  os << "struct " << exp << "access::view_traits_impl< " << type << ", " <<
    "id_" << db << " >::query_columns";

  if (obj_count > 1)
  {
    os << "{";

    for (view_objects::const_iterator i (objs.begin ()); i < objs.end (); ++i)
    {
      if (i->kind != view_object::object)
        continue; // Skip tables.

      bool alias (!i->alias.empty ());
      semantics::class_& o (*i->obj);
      string const& oname (alias ? i->alias : class_name (o));
      string const& otype (class_fq_name (o));
      qname const& table (table_name (o));

      os << "// " << oname << endl
         << "//" << endl
         << "typedef" << endl
         << "odb::pointer_query_columns<" << endl
         << "  " << otype << "," << endl
         << "  id_" << db << "," << endl;

      if (alias && (polymorphic (o) ||
                    table.qualified () ||
                    i->alias != table.uname ()))
      {
        os << "  odb::alias_traits< " << otype << "," << endl
           << "    id_" << db << "," << endl
           << "    access::view_traits_impl< " << type << ", id_" << db <<
          " >::" << i->alias << "_tag> >" << endl;
      }
      else
        os << "  odb::access::object_traits_impl< " << otype << ", id_" <<
          db << " > >" << endl;

      os << oname << ";"
         << endl;
    }

    os << "};";
  }
  else
  {
    // For a single object view we generate a shortcut without
    // an intermediate typedef.
    //
    view_object const* vo (0);
    for (view_objects::const_iterator i (objs.begin ());
         vo == 0 && i < objs.end ();
         ++i)
    {
      if (i->kind == view_object::object)
        vo = &*i;
    }

    bool alias (!vo->alias.empty ());
    semantics::class_& o (*vo->obj);
    string const& otype (class_fq_name (o));
    qname const& table (table_name (o));

    os << ":" << endl
       << "  odb::pointer_query_columns<" << endl
       << "    " << otype << "," << endl
       << "    id_" << db << "," << endl;

    if (alias && (polymorphic (o) ||
                  table.qualified () ||
                  vo->alias != table.uname ()))
    {
      os << "    odb::alias_traits<" << endl
         << "      " << otype << "," << endl
         << "      id_" << db << "," << endl
         << "      access::view_traits_impl< " << type << ", id_" <<
        db << " >::" << vo->alias << "_tag> >";
    }
    else
      os << "    odb::access::object_traits_impl< " << otype <<
        ", id_" << db << " > >";

    os << "{"
       << "};";
  }
}

void view_query_columns_type::
generate_def (type& c)
{
  view_objects& objs (c.get<view_objects> ("objects"));

  // Generate alias_traits specializations.
  //
  {
    bool false_ (false); //@@ (im)perfect forwarding
    instance<query_alias_traits> at (c, false_);

    for (view_objects::const_iterator i (objs.begin ());
         i < objs.end ();
         ++i)
    {
      if (i->kind != view_object::object)
        continue; // Skip tables.

      if (i->alias.empty ())
        continue;

      semantics::class_& o (*i->obj);
      qname const& t (table_name (o));

      // Check that the alias is not the same as the table name
      // (if this is a polymorphic object, then the alias is just
      // a prefix).
      //
      if (polymorphic (o) || t.qualified () || i->alias != t.uname ())
      {
        at->generate_def (i->alias, o, i->alias);
      }
    }
  }

  if (multi_dynamic)
    generate_inst (c);
}

void view_query_columns_type::
generate_inst (type& c)
{
  string const& type (class_fq_name (c));
  view_objects& objs (c.get<view_objects> ("objects"));

  string traits ("access::view_traits_impl< " + type + ", id_" +
                 db.string () + " >");

  // Instantiate [pointer_]query_columns.
  //
  for (view_objects::const_iterator i (objs.begin ());
       i < objs.end ();
       ++i)
  {
    if (i->kind != view_object::object)
      continue; // Skip tables.

    if (i->alias.empty ())
      continue;

    semantics::class_& o (*i->obj);
    qname const& t (table_name (o));

    // Check that the alias is not the same as the table name
    // (if this is a polymorphic object, then the alias is just
    // a prefix).
    //
    if (polymorphic (o) || t.qualified () || i->alias != t.uname ())
    {

      string const& otype (class_fq_name (o));

      // If the pointed-to class has no pointers of its own then
      // pointer_query_columns just derives from query_columns and
      // that's what we need to instantiate.
      //
      bool has_ptr (has_a (o, test_pointer | include_base));

      string alias ("odb::alias_traits<\n"
                    "    " + otype + ",\n"
                    "    id_" + db.string () + ",\n"
                    "    " + traits + "::" + i->alias + "_tag>");



      // Instantiate base [pointer_]query_columns.
      //
      {
        instance<query_columns_base_insts> b (has_ptr, decl_, alias);
        traversal::inherits i (*b);
        inherits (o, i);
      }

      inst_header (decl_);
      os << (has_ptr ? "pointer_" : "") <<
        "query_columns<" << endl
         << "  " << otype << "," << endl
         << "  id_" << db << "," << endl
         << "  " << alias << " >;"
         << endl;
    }
  }
}