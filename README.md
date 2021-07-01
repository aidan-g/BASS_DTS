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

    //Load as a plugin if you like.
    if (!BassDts.Load())
    {
        Assert.Fail("Failed to load DTS.");
    }

    //Use one of these.
    var sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
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

As of 0.2.0 (4e9f449dc0dd2b9aafecb651c0af750f6653aba0) this library is working as a BASS plugin.
This means you only need to include `bass_dts.dll` with your application (with your other codecs, likely in the `addon` folder).
One caviat is that BASS will prefer a built in codec if it finds a header, I have observed .dts files with WAVE/RIFF headers that cause BASS to play the file as wav. 
As plugin codec association is only by file extension, I don't think there's a way to prevent this behaviour.

If this is an issue to you then continue to use `BASS_DTS_StreamCreateFile/BassDts.CreateStream`.