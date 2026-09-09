// Generated from frozen CoreRuntimeState.cs by generate_sentence_settings_oracle.py.
using System;using System.IO;using System.Linq;using System.Globalization;using System.Collections.Generic;using System.Text.Json;
namespace TigerClaw.Core {
internal sealed class SentenceSettingsOracle {
 const string Yes="是",No="否";
 readonly object _lock=new object();readonly Dictionary<string,string> _config=new Dictionary<string,string>();
private const string KeyAutoEnableSentenceBySchema = "\u81ea\u52a8\u542f\u7528\u6574\u53e5\u6a21\u5f0f";
private const string KeySentenceAutoCommit = "\u6574\u53e5\u81ea\u52a8\u63d0\u524d\u4e0a\u5c4f";
private const string KeySentenceMinRetainedRawLength = "\u4fdd\u7559\u6700\u5c11\u7f16\u7801\u6570\u91cf";
private const string KeySentenceOptimalCodeHighFreqLimit = "\u9ad8\u9891\u5b57\u4ec5\u4f7f\u7528\u6700\u4f18\u7801\u7ec4\u53e5";
internal const int DefaultSentenceOptimalCodeHighFreqLimit = 1500;
private const string KeySentenceFullCodeWhitelist = "\u6574\u53e5\u5141\u8bb8\u5168\u7801\u7ec4\u53e5\u767d\u540d\u5355";
internal const string DefaultSentenceFullCodeWhitelist =
            "便深候整调脸照病增响剑哪微营修愿密脑续假值弹您球激游模静源副座喝富宣呼检救嘴税探脱误释跳睡减蒙镇域洞湾卖暴输缓熟庭俄韩混词授摆诺稳塔潜硬萧侵懂蒋赞赛胸偷烧墙爆操挑撤筑戴植援凭聚凌梁箭圈惨飘旗牌废缩碎挺晓桥赫凝潮掩拔播艘滚兽隆薄愤漫爹撒佩绕";
private const string KeySentenceAllowDuplicateSingleCharacters = "\u5141\u8bb8\u5355\u5b57\u91cd\u7801\u7ec4\u53e5";
public int GetSentenceMinRetainedRawLength()
        {
            lock (_lock)
            {
                if (_config.TryGetValue(KeySentenceMinRetainedRawLength, out string raw) &&
                    int.TryParse(raw, out int n))
                {
                    if (n < 0)
                    {
                        return 0;
                    }

                    if (n > 32)
                    {
                        return 32;
                    }

                    return n;
                }
            }

            return 0;
        }
public int GetSentenceOptimalCodeHighFreqLimit()
        {
            lock (_lock)
            {
                if (!_config.TryGetValue(KeySentenceOptimalCodeHighFreqLimit, out string raw))
                {
                    return DefaultSentenceOptimalCodeHighFreqLimit;
                }

                if (string.IsNullOrWhiteSpace(raw))
                {
                    return 0;
                }

                if (!int.TryParse(raw.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out int n) ||
                    n < 0)
                {
                    return 0;
                }

                return n;
            }
        }
public string GetSentenceFullCodeWhitelistText()
        {
            lock (_lock)
            {
                if (_config.TryGetValue(KeySentenceFullCodeWhitelist, out string raw))
                {
                    return raw ?? string.Empty;
                }
            }

            return DefaultSentenceFullCodeWhitelist;
        }
internal static ISet<string> ParseCharacterSet(string raw)
        {
            var result = new HashSet<string>(StringComparer.Ordinal);
            if (string.IsNullOrWhiteSpace(raw))
            {
                return result;
            }

            TextElementEnumerator enumerator = StringInfo.GetTextElementEnumerator(raw.Trim());
            while (enumerator.MoveNext())
            {
                string text = enumerator.GetTextElement();
                if (!string.IsNullOrWhiteSpace(text))
                {
                    result.Add(text);
                }
            }

            return result;
        }
public bool GetBool(string key, bool def)

        {

            lock (_lock) { return _config.TryGetValue(key, out string raw) ? ParseBool(raw, def) : def; }

        }
private static int FindSep(string line)

        {

            for (int i = 0; i < line.Length; i++) { if (line[i] == '\t' || line[i] == ' ' || line[i] == ',') { return i; } }

            return -1;

        }
private static bool ParseBool(string text, bool def)

        {

            if (string.IsNullOrWhiteSpace(text)) { return def; }

            string v = text.Trim();

            if (string.Equals(v, Yes, StringComparison.OrdinalIgnoreCase) || string.Equals(v, "true", StringComparison.OrdinalIgnoreCase) || string.Equals(v, "on", StringComparison.OrdinalIgnoreCase) || v == "1") { return true; }

            if (string.Equals(v, No, StringComparison.OrdinalIgnoreCase) || string.Equals(v, "false", StringComparison.OrdinalIgnoreCase) || string.Equals(v, "off", StringComparison.OrdinalIgnoreCase) || v == "0") { return false; }

            return def;

        }
 static string Hex(string s)=>string.Concat(s.Select(c=>((int)c).ToString("x4")));
 static string Token(string h)=>h=="-"?"":new string(Enumerable.Range(0,h.Length/4).Select(i=>(char)int.Parse(h.Substring(i*4,4),NumberStyles.HexNumber)).ToArray());
 public static int Run(string path) {
  CultureInfo.CurrentCulture=CultureInfo.InvariantCulture;
  foreach(var encoded in File.ReadLines(path)) {
   var instance=new SentenceSettingsOracle();var merged=instance._config;
   foreach(var raw in Token(encoded).Split(new[]{'\r','\n'})) {
                    if (string.IsNullOrWhiteSpace(raw)) { continue; }

                    string line = raw.TrimStart().TrimEnd('\r', '\n');

                    if (line.Length == 0 || line[0] == '#') { continue; }

                    int pos = FindSep(line);

                    if (pos <= 0) { continue; }

                    string key = line.Substring(0, pos).Trim();

                    string value = pos + 1 < line.Length ? line.Substring(pos + 1).Trim() : string.Empty;

                    if (key.Length == 0) { continue; }
                    // Other config keys do not affect these getters.

                    merged[key] = value;
   }
   string white=instance.GetSentenceFullCodeWhitelistText();
   Console.WriteLine(JsonSerializer.Serialize(new {
    enable=instance.GetBool(KeyAutoEnableSentenceBySchema,true),automatic=instance.GetBool(KeySentenceAutoCommit,false),
    duplicates=instance.GetBool(KeySentenceAllowDuplicateSingleCharacters,true),retained=instance.GetSentenceMinRetainedRawLength(),
    common=instance.GetSentenceOptimalCodeHighFreqLimit(),white=Hex(white),characters=ParseCharacterSet(white).OrderBy(s=>s,StringComparer.Ordinal).Select(Hex).ToArray()
   }));
  }return 0;
 }
}
}
