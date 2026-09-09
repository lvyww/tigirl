using System;
using System.Globalization;
using System.IO;
namespace TigerClaw.Core {
    internal static class NgramOracle {
        private static string Token(string hex) {
            if(hex=="-")return string.Empty;
            if(hex.Length%4!=0)throw new FormatException("UTF16 token length");
            var chars=new char[hex.Length/4];
            for(int i=0;i<chars.Length;++i)chars[i]=(char)int.Parse(hex.Substring(i*4,4),NumberStyles.HexNumber,CultureInfo.InvariantCulture);
            return new string(chars);
        }
        static int Main(string[] args) {
            try {
                if(args[0]=="--auto-commit")return AutoCommitOracle.Run(args[1]);
                if(args[0]=="--settings")return SentenceSettingsOracle.Run(args[1]);
                if(args[0]=="--decoder")return DecoderOracle.Run(args[1],args[2],args.Length>3 && args[3]=="1");
                if(args[0]=="--supplement-file")return SupplementFileOracle.Run(args[1]);
                if(args[0]=="--supplement")return SupplementOracle.Run(args[1]);
                if(args[0]=="--lexicon")return LexiconOracle.Run(args[1]);
                using(var model=SentenceNgramModel.Load(args[0]))
                foreach(var line in File.ReadLines(args[1])) {
                    var row=line.Split(' ',StringSplitOptions.RemoveEmptyEntries);
                    var a=Token(row[0]);var b=Token(row[1]);var c=Token(row[2]);
                    Console.WriteLine(model.LogProbability(a,b,c,row[3]=="1").ToString("R",CultureInfo.InvariantCulture)+" "+(model.HasObservedBigram(b,c)?"1":"0"));
                }
                return 0;
            }catch(Exception error){Console.Error.WriteLine(error.Message);return 1;}
        }
    }
}
