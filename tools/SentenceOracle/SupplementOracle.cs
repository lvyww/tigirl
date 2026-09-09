using System;
using System.IO;
using System.Linq;
using System.Globalization;
using System.Collections.Generic;
namespace TigerClaw.Core {
internal static class SupplementOracle {
 static string Token(string h)=>h=="-"?"":new string(Enumerable.Range(0,h.Length/4).Select(i=>(char)int.Parse(h.Substring(i*4,4),NumberStyles.HexNumber)).ToArray());
 public static int Run(string file){
  var entries=new List<SentenceSupplementEntry>();var queries=new List<(int,string)>();
  foreach(var line in File.ReadLines(file)){
   var r=line.Split(' ',StringSplitOptions.RemoveEmptyEntries);
   if(r[0]=="B"){entries.Clear();queries.Clear();}
   if(r[0]=="E")entries.Add(SentenceSupplementEntry.Create(Token(r[1]),long.Parse(r[2])));
   if(r[0]=="Q")queries.Add((int.Parse(r[1]),Token(r[2])));
   if(r[0]=="X"){
    var matcher=SentenceSupplementMatcher.Build(entries);int state=0;
    foreach(var q in queries){state=matcher.Advance(q.Item1==-2?state:q.Item1,q.Item2,out double reward);Console.WriteLine(state+" "+reward.ToString("R",CultureInfo.InvariantCulture));}
   }
  }return 0;
 }
}
}
