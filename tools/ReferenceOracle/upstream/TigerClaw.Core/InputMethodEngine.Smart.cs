using System;
using System.Linq;

namespace TigerClaw.Core
{
    internal sealed partial class InputMethodEngine
    {
        private bool _smartSpaceArmed;
        private string _smartSpaceSelectedText;
        private bool IsSmartSentence => (_sentenceInputDecoder?.SmartMaxCodeLength ?? 0) > 0;
        private int SmartSelectionConfigMask => (_state.GetSecondCandidateSemicolon() ? 1 : 0) |
            (_state.GetThirdCandidateQuote() ? 2 : 0);

        private int CountSentenceCodes(string raw, int start = 0) => IsSmartSentence
            ? SmartSentenceSegmentation.CountLetters(raw, start)
            : Math.Max(0, raw.Length - start);

        private void RestoreSmartSpaceSelection()
        {
            if (!IsSmartSentence || _smartSpaceSelectedText == null)
            {
                return;
            }
            int index = Array.FindIndex(_sentenceDecodeResult.Candidates ?? Array.Empty<SentenceCandidate>(),
                candidate => candidate.Text == _smartSpaceSelectedText);
            if (index >= 0)
            {
                _sentenceSelectedIndex = index;
                _sentenceManualSelectionGeneration = _sentenceGeneration;
            }
        }

        private string GetSmartSentenceDisplayCode(string raw, string remaining)
        {
            if (!SmartSentenceSegmentation.TryParse(raw, _sentenceInputDecoder.SmartMaxCodeLength, out var segments,
                _sentenceInputDecoder.SmartSelectionMask))
            {
                return remaining;
            }
            string display = string.Join(" ", segments.Where(segment => segment.End > _sentenceCommittedRawLength)
                .Select(segment => raw.Substring(Math.Max(segment.Start, _sentenceCommittedRawLength),
                    segment.End - Math.Max(segment.Start, _sentenceCommittedRawLength)).TrimEnd(' ')));
            return raw.EndsWith(" ", StringComparison.Ordinal) && display.Length > 0 ? display + " " : display;
        }
    }
}
