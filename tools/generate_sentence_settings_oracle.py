"""Extract original setting methods into a dependency-free test adapter."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'tools/SentenceOracle/upstream/TigerClaw.Core/CoreRuntimeState.cs').read_text()
def method(signature):
 start=source.index(signature);brace=source.index('{',start);depth=1;i=brace+1
 while depth:
  if source[i]=='{':depth+=1
  elif source[i]=='}':depth-=1
  i+=1
 return source[start:i]
constants=[]
for name in ['KeyAutoEnableSentenceBySchema','KeySentenceAutoCommit','KeySentenceMinRetainedRawLength','KeySentenceOptimalCodeHighFreqLimit','DefaultSentenceOptimalCodeHighFreqLimit','KeySentenceFullCodeWhitelist','DefaultSentenceFullCodeWhitelist','KeySentenceAllowDuplicateSingleCharacters']:
 import re
 m=re.search(r'(?:private|internal) const (?:string|int) '+name+r'\s*=',source);end=source.index(';',m.start())
 constants.append(source[m.start():end+1])
methods=[method(s) for s in ['public int GetSentenceMinRetainedRawLength','public int GetSentenceOptimalCodeHighFreqLimit','public string GetSentenceFullCodeWhitelistText','internal static ISet<string> ParseCharacterSet','public bool GetBool','private static int FindSep','private static bool ParseBool']]
start=source.index('                    if (string.IsNullOrWhiteSpace(raw)) { continue; }')
end=source.index('                    merged[key] = value;',start)+len('                    merged[key] = value;')
load=source[start:end].replace('if (!KnownConfigKeys.Contains(key)) { continue; }','// Other config keys do not affect these getters.')
text='''// Generated from frozen CoreRuntimeState.cs by generate_sentence_settings_oracle.py.
using System;using System.IO;using System.Linq;using System.Globalization;using System.Collections.Generic;using System.Text.Json;
namespace TigerClaw.Core {
internal sealed class SentenceSettingsOracle {
 const string Yes="是",No="否";
 readonly object _lock=new object();readonly Dictionary<string,string> _config=new Dictionary<string,string>();
'''+ '\n'.join(constants+methods)+'''
 static string Hex(string s)=>string.Concat(s.Select(c=>((int)c).ToString("x4")));
 static string Token(string h)=>h=="-"?"":new string(Enumerable.Range(0,h.Length/4).Select(i=>(char)int.Parse(h.Substring(i*4,4),NumberStyles.HexNumber)).ToArray());
 public static int Run(string path) {
  CultureInfo.CurrentCulture=CultureInfo.InvariantCulture;
  foreach(var encoded in File.ReadLines(path)) {
   var instance=new SentenceSettingsOracle();var merged=instance._config;
   foreach(var raw in Token(encoded).Split(new[]{'\\r','\\n'})) {
'''+load+'''
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
'''
(root/'tools/SentenceOracle/SentenceSettingsOracle.cs').write_text(text)
