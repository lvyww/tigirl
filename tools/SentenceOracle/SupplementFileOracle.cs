// Original method bodies copied verbatim from frozen CoreRuntimeState.cs.
// Adapter supplies UTF-8 fixtures only; encoding detection is tested separately.
using System;using System.IO;using System.Text;using System.Linq;using System.Globalization;using System.Collections.Generic;using System.Text.Json;
namespace TigerClaw.Core {
internal static class SupplementFileOracle {
 const string SentenceSupplementFileName="补充语料.txt";
 static Encoding DetectTextEncoding(string path)=>Encoding.UTF8;
 public static int Run(string directory) {
  Console.WriteLine(JsonSerializer.Serialize(LoadSentenceSupplements(directory).Select(e=>new {
   text=string.Concat(e.Text.Select(c=>((int)c).ToString("x4"))),weight=e.Weight,reward=e.Reward
  }).ToArray()));return 0;
 }
internal static SentenceSupplementEntry[] LoadSentenceSupplements(string mbDir)
        {
            if (string.IsNullOrEmpty(mbDir) || !Directory.Exists(mbDir))
            {
                return Array.Empty<SentenceSupplementEntry>();
            }

            string path = Path.Combine(mbDir, SentenceSupplementFileName);
            if (!File.Exists(path))
            {
                return Array.Empty<SentenceSupplementEntry>();
            }

            Encoding encoding = DetectTextEncoding(path);
            var entries = new Dictionary<string, SentenceSupplementEntry>(StringComparer.Ordinal);
            foreach (string raw in File.ReadLines(path, encoding))
            {
                string line = StripInlineComment(raw ?? string.Empty).Trim();
                if (line.Length == 0)
                {
                    continue;
                }

                string[] parts = line.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length < 1 || parts.Length > 2)
                {
                    continue;
                }

                string text = parts[0].Trim();
                if (text.Length == 0)
                {
                    continue;
                }

                long weight = 1000;
                if (parts.Length == 2 &&
                    (!long.TryParse(parts[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out weight) ||
                     weight <= 0))
                {
                    continue;
                }

                entries[text] = SentenceSupplementEntry.Create(text, weight);
            }

            return entries.Values.ToArray();
        }
private static string StripInlineComment(string line)

        {

            if (string.IsNullOrEmpty(line))

            {

                return string.Empty;

            }



            var sb = new StringBuilder(line.Length);

            int slashRun = 0;

            for (int i = 0; i < line.Length; i++)

            {

                char ch = line[i];

                if (ch == '#')

                {

                    if ((slashRun & 1) == 1)

                    {

                        // "\#" means literal '#': drop the escaping slash and keep '#'.

                        if (sb.Length > 0) { sb.Length -= 1; }

                        sb.Append('#');

                        slashRun = 0;

                        continue;

                    }



                    break;

                }



                sb.Append(ch);

                slashRun = (ch == '\\') ? (slashRun + 1) : 0;

            }



            return sb.ToString().Trim();

        }
}
}
