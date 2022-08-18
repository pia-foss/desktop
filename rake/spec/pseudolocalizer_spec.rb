require_relative '../product/pseudolocalizer'

# Pseudolocalization is the process of replacing English characters with more exotic counterparts
# that are still readable to an English speaker.
# This is done to verify there's no glitches when the UI renders these characters - and helps
# to identfy translation-related bugs that could appear with real translations.
RSpec.describe PseudoLocalizer do
    let(:lang_id) { "ro" }

    context "localization" do
        let(:input_string) do
            "<source>Subscription tile</source>\n<extracomment>Screen reader annotation for the Subscription tile, should reflect the name of the tile</extracomment>\n<translation type=\"unfinished\"></translation>"
        end

        let(:localized_string) do
            "<source>Subscription tile</source>\n<extracomment>Screen reader annotation for the Subscription tile, should reflect the name of the tile</extracomment>\n<translation>İЩア_Súbscríptíón tílé]</translation>"
        end

        it 'pseudolocalizes a string' do
            expect(PseudoLocalizer.pseudolocalizeString(input_string, lang_id)).to eq localized_string
        end
    end

    context "html entity" do
        let(:input_string) do
            "<source>&foobar;</source>\n<extracomment></extracomment>\n<translation type=\"unfinished\"></translation>"
        end

        # The &foobar; is unchanged by localization - however the prefix and underscores are still added
        # for UI testing.
        let(:localized_string) do
            "<source>&foobar;</source>\n<extracomment></extracomment>\n<translation>İЩ_&foobar;]</translation>"
        end

        it 'does not localize characters which are part of an html entity' do
            expect(PseudoLocalizer.pseudolocalizeString(input_string, lang_id)).to eq localized_string
        end
    end

    context "language id replacement" do
        let(:input_string) { %{<TS version="2.1" language="en_US"></TS>}}
        let(:localized_string) { %{<TS version="2.1" language="#{lang_id}"></TS>} }

        it 'replaces the language id' do
            expect(PseudoLocalizer.pseudolocalizeString(input_string, lang_id)).to eq localized_string
        end
    end
end

