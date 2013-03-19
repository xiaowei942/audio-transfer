package com.thinpad.audiotransfer;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class AudiotransferActivity extends Activity {
	@Override
	protected void onDestroy() {
		// TODO Auto-generated method stub
		unitDestroy();
		super.onDestroy();
	}

	/** Called when the activity is first created. */
	public static final int INPUT_FLAG = 1;
	public static final int OUTPUT_FLAG = 2;

	private Button btn_init;
	private Button btn_play;
	private Button btn_send;
	private Button btn_read;
	private Button btn_save;
	private Button btn_readfile;
	
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
	}

	public Button.OnClickListener listener = new OnClickListener() {

		@Override
		public void onClick(View arg0) {
			// TODO Auto-generated method stub
			switch (arg0.getId()) {
			case R.id.btn_init:
				unitInit(44100, 1, 44100, 2, (OUTPUT_FLAG | INPUT_FLAG));
				break;
			case R.id.btn_send:
				sendMessage("test".getBytes(), "test".getBytes().length);
				break;
			case R.id.btn_save:
				//testSaveData();
				byte[] tmp = new byte[1024];
				int ret = receiveData(tmp,5);
				if(ret>0){
					for(int i=0; i<ret; i++){
						System.out.println(tmp[i]);
					}
				} else if(ret==0){
					System.out.println("接收异常");
				} else  //返回 -1
					System.out.println("超时");
				
					
				break;
			default:
				break;
			}
		}
	};

	public static native int unitInit(int play_rate, int play_channels,
			int rec_rate, int rec_channels, int flags);
	public static native void unitDestroy();
	public static native int receiveData(byte[] recv, int timeout);
	public static native int sendMessage(byte[] str, int length);

	static {
		System.loadLibrary("transfer");
	}
}