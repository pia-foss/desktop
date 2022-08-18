require_relative '../util/cppheader.rb'

#
# The CppHeader class supports the creation of simple C++ headers
# that defining values using macros. We use it to move data from
# the build system into the build, like the version number or the brand.
# It ensures the use of include guards to avoid double inclusion.
#
describe CppHeader do

    it "creates empty header file with correct guards" do
        header = CppHeader.new("emptyinit")
        expect(header.content).to eq "#ifndef EMPTYINIT_H\n#define EMPTYINIT_H\n\n#endif\n"
    end

    it "creates a header file with correct guards and adds the line" do
        header = CppHeader.new("simpleheader")
        header.line "std::string simple = \"line\""
        expect(header.content).to eq "#ifndef SIMPLEHEADER_H\n#define SIMPLEHEADER_H\n\nstd::string simple = \"line\"\n#endif\n"
    end

    it "creates a header file with correct guards the specified values" do
        header = CppHeader.new("simpleheader")
        header.defineString "String", "StringValue"
        header.defineRawString "RawString", "RawStringValue"
        header.defineLiteral "Literal", "LiteralValue"
        expect(header.content).to eq "#ifndef SIMPLEHEADER_H\n#define SIMPLEHEADER_H\n\n#define String \"StringValue\"\n#define RawString R\"(RawStringValue)\"\n#define Literal LiteralValue\n#endif\n"
    end
end