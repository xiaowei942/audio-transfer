package com.thinpad.audiotransfer;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.util.Log;

public class AudiotransferActivity extends Activity {
	/** Called when the activity is first created. */
	public static final int INPUT_FLAG = 1;
	public static final int OUTPUT_FLAG = 2;

	private Button btn_init;
	private Button btn_play;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		btn_init = (Button) findViewById(R.id.btn_init);
		btn_play = (Button) findViewById(R.id.btn_play);
		btn_init.setOnClickListener(listener);
		btn_play.setOnClickListener(listener);
	}

	public Button.OnClickListener listener = new OnClickListener() {

		@Override
		public void onClick(View arg0) {
			// TODO Auto-generated method stub
			switch (arg0.getId()) {
			case R.id.btn_init:
				unitInit(44100, 2, 44100, 2, (OUTPUT_FLAG | INPUT_FLAG));
				break; 
			case R.id.btn_play:
				doPlay();
				break;

			default:
				break;
			}
		}
	};

	public static native int unitInit(int play_rate, int play_channels,
			int rec_rate, int rec_channels, int flags);

	public static native int doPlay();

	static {
		System.loadLibrary("transfer");
	}
}