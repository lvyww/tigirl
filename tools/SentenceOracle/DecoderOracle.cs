using System;
using System.IO;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json;
namespace TigerClaw.Core {
internal static class DecoderOracle {
 static string Token(string h)=>h=="-"?"":new string(Enumerable.Range(0,h.Length/4).Select(i=>(char)int.Parse(h.Substring(i*4,4),NumberStyles.HexNumber)).ToArray());
 static string Hex(string s)=>string.Concat(s.Select(c=>((int)c).ToString("x4")));
 public static int Run(string file,string modelPath,bool incremental) {
  using var model=modelPath=="-"?null:SentenceNgramModel.Load(modelPath);
  var source=new Dictionary<string,List<string>>(StringComparer.Ordinal);
  var common=new HashSet<string>();var white=new HashSet<string>();
  var supplements=new List<SentenceSupplementEntry>();var queries=new List<(string raw,string prefix)>();
  string[] o=null;int id=0;
  foreach(var line in File.ReadLines(file)) {
   var r=line.Split(' ',StringSplitOptions.RemoveEmptyEntries);if(r.Length==0)continue;
   if(r[0]=="B"){id=int.Parse(r[1]);source.Clear();common.Clear();white.Clear();queries.Clear();supplements.Clear();}
   if(r[0]=="C")foreach(var v in r.Skip(1))common.Add(Token(v));
   if(r[0]=="W")foreach(var v in r.Skip(1))white.Add(Token(v));
   if(r[0]=="E")source[Token(r[1])]=r.Skip(2).Select(Token).ToList();
   if(r[0]=="S")supplements.Add(SentenceSupplementEntry.Create(Token(r[1]),long.Parse(r[2])));
   if(r[0]=="O")o=r;
   if(r[0]=="Q")queries.Add((Token(r[1]),r.Length>2?Token(r[2]):""));
   if(r[0]!="X")continue;
   double D(int n)=>double.Parse(o[n],CultureInfo.InvariantCulture);int I(int n)=>int.Parse(o[n]);
   var decoder=new SentenceInputDecoder(SentenceLexiconIndex.Build(source,common,white),model,
    I(1),D(2),new SentenceIsolationPenalty{RankThreshold=I(3),Lambda=D(4),UseLogRank=I(5)!=0},
    I(6)!=0,D(7),D(8),SentenceSupplementMatcher.Build(supplements),I(9)!=0);
   int number=0;
   foreach(var q in queries) {
    int n=number++;int limit=n%5==0?1:I(10);bool evidence=n%3!=0;
    if(n%17==0)decoder.ResetDecodeCache();
    try {
     var result=incremental?decoder.Decode(q.raw,limit,evidence,q.prefix):decoder.DecodeFull(q.raw,limit,evidence,q.prefix);
     Console.WriteLine(JsonSerializer.Serialize(new {id,query=Hex(q.raw),raw=Hex(result.RawCode),expanded=result.ExpandedStates,
      evidence=Evidence(result.EarlyCommitEvidence),exists=new[]{decoder.HasCompleteCandidate(q.raw),decoder.HasCompleteCandidate(q.raw,q.prefix),decoder.HasCompleteCandidate(q.raw,q.prefix,result.Candidates.FirstOrDefault()?.Text??"",true)},properPrefix=decoder.IsProperCodePrefix(q.raw),candidates=result.Candidates.Select(c=>new {text=Hex(c.Text),segmented=Hex(c.SegmentedCode),score=c.FinalScore,
       confidence=c.ConfidenceScore,supplement=c.SupplementScore,rank=c.MaxLexiconRank,
       boundaries=Boundaries(c.Boundary)}).ToArray()}));
    }catch(FormatException){Console.WriteLine(JsonSerializer.Serialize(new{id,query=Hex(q.raw),error="selector"}));}
    catch(OverflowException){Console.WriteLine(JsonSerializer.Serialize(new{id,query=Hex(q.raw),error="selector"}));}
   }
  }return 0;
 }
 static object Evidence(SentenceEarlyCommitEvidence e)=>new {
  prefixes=e.Prefixes.Select(p=>new{text=Hex(p.Text),raw=p.RawLength,share=p.Share,boundaryShare=p.BoundaryShare,closed=p.BoundaryClosed}).ToArray(),
  neutralIncomplete=e.NeutralIncompleteTail,merged=e.MergedIncompleteTail,low=e.NeutralLowConfidence,truncated=e.ConfidenceTruncated,
  proposal=Hex(e.Proposal),share=e.ProposalShare,rawLengths=e.RawLengths.ToDictionary(p=>Hex(p.Key),p=>p.Value)
 };
 static int[][] Boundaries(SentencePathBoundary b) {
  var result=new List<int[]>();for(;b!=null;b=b.Previous)result.Add(new[]{b.TextLength,b.RawLength});return result.ToArray();
 }
}
}
