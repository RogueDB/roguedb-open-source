using System;
using System.Threading;
using Microsoft.Extensions.Logging;

namespace GettingStarted.GrpcPlaceholder
{
    class Program
    {
        static int Main(string[] args)
        {
            using var loggerFactory = LoggerFactory.Create(builder => builder.AddSimpleConsole());
            var logger = loggerFactory.CreateLogger<Program>();

            logger.LogInformation("gRPC placeholder starting.");
            Console.WriteLine("This is a placeholder gRPC program. Replace with real server/client startup code.");

            if (args.Length > 0 && args[0].Equals("server", StringComparison.OrdinalIgnoreCase))
            {
                logger.LogInformation("Running server placeholder. Replace with Grpc.AspNetCore server setup.");
                Console.WriteLine("Server placeholder running. Press Ctrl+C to exit.");
                Thread.Sleep(Timeout.Infinite); // keep process alive for testing
            }
            else
            {
                logger.LogInformation("Running client placeholder. Replace with Grpc.Net.Client client calls.");
                Console.WriteLine("Client placeholder executed.");
            }

            logger.LogInformation("gRPC placeholder exiting.");
            return 0;
        }
    }
}