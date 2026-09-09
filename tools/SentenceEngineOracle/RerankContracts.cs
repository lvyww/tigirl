// Original request/interface declarations only. No service is created.
using System;
namespace TigerClaw.Core {
    internal sealed class SentenceRerankRequest
    {
        public long Generation { get; set; }
        public string RawCode { get; set; }
        public string[] Candidates { get; set; }
    }

    internal interface ISentenceRerankService : IDisposable
    {
        void Request(SentenceRerankRequest request);
    }

}
