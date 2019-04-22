# BASS_DTS

A DTS decoder plugin for BASS which uses the libdcadec library with .NET bindings.

bass.dll is required for native projects.
ManagedBass is required for .NET projects.

A simple example;

```c
public void Main()
{
    if (!Bass.Init(Bass.DefaultDevice))
    {
        Assert.Fail(string.Format("Failed to initialize BASS: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
    }

    //Plugin is not yet working.
    //if (Bass.PluginLoad(Path.Combine(CurrentDirectory, "bass_dts.dll")) == 0)
    //{
    //    Assert.Fail("Failed to load DTS.");
    //}

    var sourceChannel = BassDts.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
    if (sourceChannel == 0)
    {
        Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
    }

    var channelInfo = default(ChannelInfo);
    if (!Bass.ChannelGetInfo(sourceChannel, out channelInfo))
    {
        Assert.Fail(string.Format("Failed to get stream info: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
    }

    if (!Bass.ChannelPlay(sourceChannel))
    {
        Assert.Fail(string.Format("Failed to play the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
    }

    var channelLength = Bass.ChannelGetLength(sourceChannel);
    var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

    do
    {
        if (Bass.ChannelIsActive(sourceChannel) == PlaybackState.Stopped)
        {
            break;
        }

        var channelPosition = Bass.ChannelGetPosition(sourceChannel);
        var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

        Debug.WriteLine(
            "{0}/{1}",
            TimeSpan.FromSeconds(channelPositionSeconds).ToString("g"),
            TimeSpan.FromSeconds(channelLengthSeconds).ToString("g")
        );

        Thread.Sleep(1000);
    } while (true);

    if (!Bass.StreamFree(sourceChannel))
    {
        Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
    }

    if (!Bass.Free())
    {
        Assert.Fail(string.Format("Failed to free BASS: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
    }
}
```

Unfortunately I can't get the "plugin" aspect working correctly.
There is a way to associate .dts files with the plugin so that BASS_StreamCreateFile works.
You must use BASS_DTS_StreamCreateFile for now.
