#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/compiler/cpp/generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>

using namespace google::protobuf::compiler;
using namespace google::protobuf;
using std::string_view;
using std::string;

using cstr = std::string const;


void println(auto&&... args) {
  (std::cerr << ... << args) << std::endl;
}


struct FileGenerator {
  GeneratorContext* context;
  std::unique_ptr<io::ZeroCopyOutputStream> output;
  std::unique_ptr<io::Printer> printer;

  void generate(FileDescriptor const* file) {
    output.reset(context->Open("bob.hpp"));
    printer = std::make_unique<io::Printer>(output.get(), '$', nullptr);

    printer->Print(
      "// Generated by the protobuf-cpp compiler. DO NOT EDIT!\n"
      "// Source: $filename$\n"
      "#include <protobuf-cpp.hpp>\n\n",
      "filename", file->name());

    // create the namespace
    string ns;
    string_view pkg = file->package();
    for (;;) {
      if (auto pos = pkg.find('.'); pos != string::npos) {
        ns += (string)pkg.substr(0,pos) + "::";
        pkg = pkg.substr(pos+1);
      } else {
        ns += (string)pkg;
        break;
      }
    }

    // process each message type
    if (!ns.empty()) {
      printer->Print("\nnamespace $ns$ {\n", "ns", ns);
      printer->Indent();
    }
    for (int i=0; i<file->message_type_count(); i++) {
      generateStruct(file->message_type(i));
    }
    if (!ns.empty()) {
      printer->Outdent();
      printer->Print("}\n");
    }

    // generate the reflection data
    printer->Print("\n\nnamespace pbcpp {\n");
    printer->Indent();
    for (int i=0; i<file->message_type_count(); i++) {
      generateReflection(file->message_type(i), ns);
    }
    printer->Outdent();
    printer->Print("}\n");
  }

  void generateStructs(auto container) {
  }

  void generateStruct(Descriptor const* msg) {
    printer->Print("struct $name$ {\n", "name", msg->name());
    printer->Indent();

    for (int i=0; i<msg->field_count(); i++) {
      auto field = msg->field(i);

      // check for various things that still need to be supported
      if (field->containing_oneof())  error_unsupported("oneof");

      // get the type
      string cpptype;
      switch (field->type()) {
        case FieldDescriptor::TYPE_DOUBLE:   cpptype = "double"; break;
        case FieldDescriptor::TYPE_FLOAT:    cpptype = "float"; break;
        case FieldDescriptor::TYPE_INT64:    cpptype = "int64_t"; break;
        case FieldDescriptor::TYPE_UINT64:   cpptype = "uint64_t"; break;
        case FieldDescriptor::TYPE_INT32:    cpptype = "int32_t"; break;
        case FieldDescriptor::TYPE_FIXED64:  cpptype = "int64_t"; break;
        case FieldDescriptor::TYPE_FIXED32:  cpptype = "int32_t"; break;
        case FieldDescriptor::TYPE_BOOL:     cpptype = "bool"; break;
        case FieldDescriptor::TYPE_STRING:   cpptype = "std::string"; break;
        case FieldDescriptor::TYPE_GROUP:    error_will_not_support("TYPE_GROUP"); break;
        case FieldDescriptor::TYPE_MESSAGE:  cpptype = field->type_name(); break;
        case FieldDescriptor::TYPE_BYTES:    cpptype = "cbpp::bytes"; break;
        case FieldDescriptor::TYPE_UINT32:   cpptype = "uint32_t"; break;
        case FieldDescriptor::TYPE_ENUM:     cpptype = field->type_name(); break;
        case FieldDescriptor::TYPE_SFIXED32: cpptype = "int32_t"; break;
        case FieldDescriptor::TYPE_SFIXED64: cpptype = "int64_t"; break;
        case FieldDescriptor::TYPE_SINT32:   cpptype = "int32_t"; break;
        case FieldDescriptor::TYPE_SINT64:   cpptype = "int64_t"; break;
      }

      // modifiers
      if (field->has_optional_keyword()) {
        cpptype = "std::optional<" + cpptype + ">";
      } else if (field->is_required()) {
        error_unsupported("required");
      } else if (field->has_optional_keyword()) {
        cpptype = "std::vector<" + cpptype + ">";
      }

      switch (field->label()) {
        case FieldDescriptor::LABEL_REQUIRED:  error_unsupported("required"); break;
        case FieldDescriptor::LABEL_REPEATED:  cpptype = "std::vector<" + cpptype + ">"; break;
        case FieldDescriptor::LABEL_OPTIONAL:  break;
      }

      printer->Print("$cpptype$ $name$;\n", "cpptype", cpptype, "name", field->name());
    }

    // generate the nested types
    for (int i=0; i<msg->nested_type_count(); i++) {
      generateStruct(msg->nested_type(i));
    }

    printer->Outdent();
    printer->Print("};\n");
  }


  void generateReflection(Descriptor const* msg, cstr& baseName) {
    string msgname = baseName + "::" + msg->name();
    string comma = ",";

    printer->Print("template <> struct reflect<$name$> : fields<\n", "name", msgname);
    printer->Indent();
    for (int i=0; i<msg->field_count(); i++) {
      auto field = msg->field(i);

      // resolve the type
      string type;
      switch (field->type()) {
        case FieldDescriptor::TYPE_DOUBLE:   type = "TYPE_DOUBLE"; break;
        case FieldDescriptor::TYPE_FLOAT:    type = "TYPE_FLOAT"; break;
        case FieldDescriptor::TYPE_INT64:    type = "TYPE_INT64"; break;
        case FieldDescriptor::TYPE_UINT64:   type = "TYPE_UINT64"; break;
        case FieldDescriptor::TYPE_INT32:    type = "TYPE_INT32"; break;
        case FieldDescriptor::TYPE_FIXED64:  type = "TYPE_FIXED64"; break;
        case FieldDescriptor::TYPE_FIXED32:  type = "TYPE_FIXED32"; break;
        case FieldDescriptor::TYPE_BOOL:     type = "TYPE_BOOL"; break;
        case FieldDescriptor::TYPE_STRING:   type = "TYPE_STRING"; break;
        case FieldDescriptor::TYPE_GROUP:    type = "TYPE_GROUP"; break;
        case FieldDescriptor::TYPE_MESSAGE:  type = "TYPE_MESSAGE"; break;
        case FieldDescriptor::TYPE_BYTES:    type = "TYPE_BYTES"; break;
        case FieldDescriptor::TYPE_UINT32:   type = "TYPE_UINT32"; break;
        case FieldDescriptor::TYPE_ENUM:     type = "TYPE_ENUM"; break;
        case FieldDescriptor::TYPE_SFIXED32: type = "TYPE_SFIXED32"; break;
        case FieldDescriptor::TYPE_SFIXED64: type = "TYPE_SFIXED64"; break;
        case FieldDescriptor::TYPE_SINT32:   type = "TYPE_SINT32"; break;
        case FieldDescriptor::TYPE_SINT64:   type = "TYPE_SINT64"; break;
      }

      // write the field
      if ((i+1) == msg->field_count())  comma = "";
      printer->Print("field<$type$, \"$name$\", $num$, &$msgname$::$name$>$comma$\n",
        "name", field->name(), "num", std::to_string(field->number()), "msgname", msgname,
        "type", type, "comma", comma);
    }
    printer->Outdent();
    printer->Print("> {};\n\n");

    // generate the nested types
    for (int i=0; i<msg->nested_type_count(); i++) {
      generateReflection(msg->nested_type(i), msgname);
    }
  }


  // ---- errors
  void error_will_not_support(string_view sv) {
    throw std::runtime_error("feature will not be supported: " + (string)sv);
  }
  void error_unsupported(string_view sv) {
    throw std::runtime_error("feature is not yet supported: " + (string)sv);
  }
};


struct MyCodeGenerator : CodeGenerator {

  bool Generate(const FileDescriptor* file,
    const std::string& parameter,
    GeneratorContext* context,
    std::string* error) const override
  {
    try {
      FileGenerator fgen;
      fgen.context = context;
      fgen.generate(file);
    } catch (std::exception const& ex) {
      *error = ex.what();
      return false;
    }
    return true;
  }
};


int main(int argc, char** argv) {
  MyCodeGenerator generator;
  return PluginMain(argc, argv, &generator);
}