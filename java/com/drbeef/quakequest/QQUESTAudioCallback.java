package com.drbeef.quakequest;

import java.nio.ByteBuffer;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;

public class QQUESTAudioCallback {

	public QuakeQuestAudioTrack mAudioTrack;	
	public ScheduledThreadPoolExecutor stpe;
	byte[] mAudioData;
	public static boolean reqThreadrunning=true;	
	public void initAudio(int size)
	{
		if(mAudioTrack != null) return;		
		size/=8;		
		
		mAudioData=new byte[size];		
		int sampleFreq = 44100;							
	
		int bufferSize = Math.max(size,AudioTrack.getMinBufferSize(sampleFreq, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT));
		mAudioTrack = new QuakeQuestAudioTrack(AudioManager.STREAM_MUSIC,
				sampleFreq,
				AudioFormat.CHANNEL_OUT_STEREO,
				AudioFormat.ENCODING_PCM_16BIT,
				bufferSize,
				AudioTrack.MODE_STREAM);
		mAudioTrack.play();
		long sleeptime=(size*1000000000l)/(2*2*sampleFreq);
		stpe=new ScheduledThreadPoolExecutor(5);
		stpe.scheduleAtFixedRate(new Runnable() {			
			@Override
			public void run() {
				if (reqThreadrunning)
				{						
					GLES3JNILib.requestAudioData( );
				}							
			}
		}, 0, sleeptime, TimeUnit.NANOSECONDS);		
	}
	
	int sync=0;

	public void writeAudio(ByteBuffer audioData, int offset, int len)
	{
		if(mAudioTrack == null)
			return;			
		if (!reqThreadrunning)
			return;
		audioData.position(offset);
		audioData.get(mAudioData, 0, len);
		if (sync++%128==0)
		mAudioTrack.flush();
		mAudioTrack.write(mAudioData, 0, len);
	}
	
	public void pauseAudio()
	{
		if(mAudioTrack == null)
			return;				
		
		mAudioTrack.pause();	
		mAudioTrack.flush();
		reqThreadrunning=false;
	}
	
	public void resumeAudio()
	{
		if(mAudioTrack == null)
			return;				

		mAudioTrack.play();	
		reqThreadrunning=true;
	}
	
	public void terminateAudio()
	{
		mAudioTrack.flush();
		mAudioTrack.release();

		mAudioTrack = null;
		
		reqThreadrunning=false;
		
		stpe.shutdown();
		stpe = null;
	}
}

class QuakeQuestAudioTrack extends AudioTrack
{	
	public QuakeQuestAudioTrack(int streamType, int sampleRateInHz,
			int channelConfig, int audioFormat, int bufferSizeInBytes, int mode)
			throws IllegalStateException {
		super(streamType, sampleRateInHz, channelConfig, audioFormat,
				bufferSizeInBytes, mode);
	}
	
	@Override
	public void play() throws IllegalStateException {				
		flush();		
		super.play();		
	}
}
