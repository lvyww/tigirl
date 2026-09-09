using System;
using System.IO;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json;
namespace TigerClaw.Core {
internal static class LexiconOracle {
    static string Token(string h)=>h=="-"?"":new string(Enumerable.Range(0,h.Length/4).Select(i=>(char)int.Parse(h.Substring(i*4,4),NumberStyles.HexNumber)).ToArray());
    static string Hex(string s)=>string.Concat(s.Select(c=>((int)c).ToString("x4")));
    public static int Run(string input) {
        var source=new Dictionary<string,List<string>>(StringComparer.Ordinal);
        var common=new HashSet<string>();var white=new HashSet<string>();var queries=new List<string>();int id=0;
        foreach(var line in File.ReadLines(input)) {
            var r=line.Split(' ',StringSplitOptions.RemoveEmptyEntries);if(r.Length==0)continue;
            if(r[0]=="B"){id=int.Parse(r[1]);source.Clear();common.Clear();white.Clear();queries.Clear();}
            if(r[0]=="C")foreach(var v in r.Skip(1))common.Add(Token(v));
            if(r[0]=="W")foreach(var v in r.Skip(1))white.Add(Token(v));
            if(r[0]=="E")source[Token(r[1])]=r.Skip(2).Select(Token).ToList();
            if(r[0]=="Q")queries.Add(Token(r[1]));
            if(r[0]=="X") {
                var index=SentenceLexiconIndex.Build(source,common,white);
                Console.WriteLine(JsonSerializer.Serialize(new {id,lengths=index.CodeLengths,queries=queries.Select(q=>new {
                    code=Hex(q),prefix=index.IsProperCodePrefix(q),candidates=(index.GetCandidates(q)??Array.Empty<SentenceLexiconCandidate>()).Select(c=>new {
                        text=Hex(c.Text),rank=c.Rank,log=c.LogRank,optimal=c.IsOptimalSingleCharacterCode,elements=c.TextElements.Select(Hex).ToArray()
                    }).ToArray()
                }).ToArray()}));
            }
        }
        return 0;
    }
}
}
