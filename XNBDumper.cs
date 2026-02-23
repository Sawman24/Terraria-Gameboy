using System;
using System.IO;
using System.Collections.Generic;
using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using System.Windows.Forms;

namespace XNBDumper
{
    class Program
    {
        static void Main(string[] args)
        {
            string inputDir = @"c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content";
            string outputDir = @"c:\Users\sawye\Desktop\Terraria Beta\DumpedAssets";

            if (!Directory.Exists(outputDir)) Directory.CreateDirectory(outputDir);

            // Need a GraphicsDevice
            Form form = new Form();
            GraphicsDevice device = null;
            try
            {
                PresentationParameters pp = new PresentationParameters();
                pp.BackBufferWidth = 1;
                pp.BackBufferHeight = 1;
                pp.DeviceWindowHandle = form.Handle;
                device = new GraphicsDevice(GraphicsAdapter.DefaultAdapter, GraphicsProfile.HiDef, pp);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Failed to create GraphicsDevice: " + ex.Message);
                return;
            }

            // Mock ContentManager
            ContentManager content = new ContentManager(new MockServiceProvider(device), inputDir);

            foreach (string file in Directory.GetFiles(Path.Combine(inputDir, "Images"), "*.xnb", SearchOption.AllDirectories))
            {
                string assetName = file.Substring(inputDir.Length + 1).Replace(".xnb", "");
                // XNB files in subdirectories need relative paths from Content
                // e.g. "Images/Item_1"
                
                Console.WriteLine("Processing " + assetName + "...");
                try
                {
                    Texture2D texture = content.Load<Texture2D>(assetName);
                    string outFile = Path.Combine(outputDir, assetName.Replace("/", "_").Replace("\\", "_") + ".png");
                    using (Stream stream = File.Create(outFile))
                    {
                        texture.SaveAsPng(stream, texture.Width, texture.Height);
                    }
                    texture.Dispose();
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Error: " + ex.Message);
                }
            }
        }
    }

    class MockServiceProvider : IServiceProvider
    {
        private IGraphicsDeviceService _graphicsDeviceService;
        public MockServiceProvider(GraphicsDevice device)
        {
            _graphicsDeviceService = new MockGraphicsDeviceService(device);
        }
        public object GetService(Type serviceType)
        {
            if (serviceType == typeof(IGraphicsDeviceService)) return _graphicsDeviceService;
            return null;
        }
    }

    class MockGraphicsDeviceService : IGraphicsDeviceService
    {
        public MockGraphicsDeviceService(GraphicsDevice device) { GraphicsDevice = device; }
        public GraphicsDevice GraphicsDevice { get; private set; }
        public event EventHandler<EventArgs> DeviceCreated;
        public event EventHandler<EventArgs> DeviceDisposing;
        public event EventHandler<EventArgs> DeviceReset;
        public event EventHandler<EventArgs> DeviceResetting;
    }
}
