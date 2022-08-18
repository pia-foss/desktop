require_relative '../util/util.rb'

#
# The Util class offers some simple public utilities
#
describe Util do

    context "two empty arrays" do
        it "cannot be joined" do
            expect(Util.joinPathArrays([],[])).to eq []
        end
    end

    context "cannot join" do
        it "empty parent" do
            expect(Util.joinPathArrays([], ["child"])).to eq []
        end

        it "empty child" do
            expect(Util.joinPathArrays(["parent"], [])).to eq []
        end
    end

    context "path joining" do
        it "join to a single path separated by a slash" do
            expect(Util.joinPathArrays(["parent"], ["child"])).to eq ["parent/child"]
        end

        it "join to #parents paths separated by a slash" do
            expect(Util.joinPathArrays(["parent1", "parent2"], ["child"])).to eq ["parent1/child", "parent2/child"]
        end

        it "join to #child paths separated by a slash" do
            expect(Util.joinPathArrays(["parent"], ["child1", "child2"])).to eq ["parent/child1", "parent/child2"]
        end

        it "join to #parents * #child paths separated by a slash" do
            expect(Util.joinPathArrays(["parent1", "parent2"], ["child1", "child2"])).to eq ["parent1/child1", "parent1/child2", "parent2/child1", "parent2/child2"]
        end
    end

    context "Joining paths" do
        it "cannot be joined" do
            expect(Util.joinPaths([[], []])).to eq []
        end

        it "a simple pair of paths" do
            expect(Util.joinPaths([["a"], ["b"]])).to eq ["a/b"]
        end

        it "a compound pair of paths" do
            expect(Util.joinPaths([["a/b"], ["c"]])).to eq ["a/b/c"]
        end

        it "three simple paths" do
            expect(Util.joinPaths([["a"], ["b"], ["c"]])).to eq ["a/b/c"]
        end

        it "a path with wildcards" do
            expect(Util.joinPaths([["a/*"], ["c"]])).to eq ["a/*/c"]
        end
    end

    context "looking for things in an array" do
        it "number is found" do
            expect(Util.find([1,2,3,4]) {|v| v == 2}).to eq 2
        end
        it "number is not found" do
            expect(Util.find([1,2,3,4]) {|v| v == 5}).to eq nil
        end

        it "string is found" do
            expect(Util.find(["One", "Two", "Three", "Four"]) {|v| v.start_with?('F')}).to eq "Four"
        end
        it "string is not found" do
            expect(Util.find(["One", "Two", "Three", "Four"]) {|v|v.equal?('Five')}).to eq nil
        end
    end

    it "returns default symbol" do
        ENV["TEST_VAR"] = ""
        expect(Util.selectSymbol("TEST_VAR", :default, [:default, :option1, :option2])).to eq :default
    end

    it "raises an error" do
        ENV["TEST_VAR"] = "invalid"
        expect{Util.selectSymbol("TEST_VAR", :default, [:default, :option1, :option2])}.to raise_error(RuntimeError)
    end

    it "returns chosen symbol" do
        ENV["TEST_VAR"] = "option1"
        expect(Util.selectSymbol("TEST_VAR", :default, [:default, :option1, :option2])).to eq :option1
    end


    context "nothing to delete" do
        it "empty prefix" do
            expect(Util.deletePrefix("prefixed", "")).to eq "prefixed"
        end

        it "no prefix match" do
            expect(Util.deletePrefix("prefixed", "erp")).to eq "prefixed"
        end

        it "empty suffix" do
            expect(Util.deleteSuffix("suffixed", "")).to eq "suffixed"
        end

        it "no suffix match" do
            expect(Util.deleteSuffix("suffixed", "dex")).to eq "suffixed"
        end
    end

    it "deletes a prefix" do
        expect(Util.deletePrefix("prefixed", "pre")).to eq "fixed"
    end

    it "deletes a suffix" do
        expect(Util.deleteSuffix("suffixed", "ixed")).to eq "suff"
    end
end