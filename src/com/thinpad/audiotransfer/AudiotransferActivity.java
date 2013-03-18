package com.thinpad.audiotransfer;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.text.format.Time;
import android.util.Log;

public class AudiotransferActivity extends Activity {
	/** Called when the activity is first created. */
	public static final int INPUT_FLAG = 1;
	public static final int OUTPUT_FLAG = 2;

	private Button btn_init;
	private Button btn_play;
	private Button btn_send;
	private Button btn_read;
	private Button btn_save;
	private Button btn_readfile;

	AnalysisData analysisdata = new AnalysisData();
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		btn_init = (Button) findViewById(R.id.btn_init);
		btn_play = (Button) findViewById(R.id.btn_play);
		btn_send = (Button) findViewById(R.id.btn_send);
		btn_read = (Button) findViewById(R.id.btn_read);
		btn_save = (Button) findViewById(R.id.btn_save);
		btn_readfile = (Button) findViewById(R.id.btn_readfile);
		btn_init.setOnClickListener(listener);
		btn_play.setOnClickListener(listener);
		btn_send.setOnClickListener(listener);
		btn_read.setOnClickListener(listener);
		btn_save.setOnClickListener(listener);
		btn_readfile.setOnClickListener(listener);
		
		Thread trd = new Thread(analysisdata);
		trd.start();
	}

	public Button.OnClickListener listener = new OnClickListener() {

		@Override
		public void onClick(View arg0) {
			// TODO Auto-generated method stub
			switch (arg0.getId()) {
			case R.id.btn_init:
				unitInit(44100, 1, 44100, 2, (OUTPUT_FLAG | INPUT_FLAG));
				break;
			case R.id.btn_play:
				//testSend();
				transferOneFrame();
				break;
			case R.id.btn_send:
				sendMessage("test".getBytes(), "test".getBytes().length);
				break;
			case R.id.btn_read:
				testRecordData();
				break;
			case R.id.btn_save:
				testSaveData();
				break;
			case R.id.btn_readfile:
				Date dt= new Date();
				  Long time= dt.getTime();//这就是距离1970年1月1日0点0分0秒的毫秒数
				  System.out.println(System.currentTimeMillis());//与上面的相同
				testReadFile();
				Date dt2= new Date();
				  Long time2= dt2.getTime();//这就是距离1970年1月1日0点0分0秒的毫秒数
				  System.out.println(System.currentTimeMillis());//与上面的相同
				break;
			default:
				break;
			}
		}
	};

	public static native int unitInit(int play_rate, int play_channels,
			int rec_rate, int rec_channels, int flags);

	public static native int testSend();
	public static native int transferOneFrame();
	public static native int testRecordData();
	public static native int testSaveData();
	public static native int testReadFile();
	public static native int sendMessage(byte[] str, int length);
	public static native int createThread();

	static {
		System.loadLibrary("transfer");
	}
}