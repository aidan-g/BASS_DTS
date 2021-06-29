using System.IO;
using System.Runtime.InteropServices;

namespace ManagedBass.Dts
{
    public static class BassDts
    {
        const string DllName = "bass_dts";

        public const ChannelType ChannelType = (ChannelType)0x1f200;

        private static int Handle;

        public static bool Load()
        {
            if (Handle != 0)
            {
                return true;
            }
            var directoryName = Path.GetDirectoryName(typeof(BassDts).Assembly.Location);
            var fileName = Path.Combine(directoryName, DllName + ".dll");
            Handle = Bass.PluginLoad(fileName);
            if (Handle == 0)
            {
                return false;
            }
            return true;
        }

        public static bool Unload()
        {
            if (Handle != 0)
            {
                var result = Bass.PluginFree(Handle);
                Handle = 0;
                return result;
            }
            return true;
        }

        [DllImport(DllName)]
        static extern int BASS_DTS_StreamCreateFile(bool Memory, string File, long Offset, long Length, BassFlags Flags);

        public static int CreateStream(string File, long Offset = 0, long Length = 0, BassFlags Flags = BassFlags.Default)
        {
            return BASS_DTS_StreamCreateFile(false, File, Offset, Length, Flags);
        }
    }
}
