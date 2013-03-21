package com.thinpad.audiotransfer;

import android.R.integer;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
 
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

	private EditText edit1;
	private Button btn_init;
	private Button btn_send;
	private Button btn_receive;
	private Button btn_save;
	
	String text= new String();
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		edit1 = (EditText) findViewById(R.id.editText1);
		btn_init = (Button) findViewById(R.id.btn_init);
		btn_send = (Button) findViewById(R.id.btn_send);
		btn_receive = (Button) findViewById(R.id.btn_receive);
		btn_save = (Button) findViewById(R.id.btn_save);
		btn_init.setOnClickListener(listener);
		btn_send.setOnClickListener(listener);
		btn_receive.setOnClickListener(listener);
		btn_save.setOnClickListener(listener);
	}

	public Button.OnClickListener listener = new OnClickListener() {

		@Override
		public void onClick(View arg0) {
			// TODO Auto-generated method stub
			switch (arg0.getId()) {
			case R.id.btn_init:
				
				int ret = unitInit(44100, 1, 44100, 2, (OUTPUT_FLAG | INPUT_FLAG));
			
				if(ret == 1)
				{
					setTitle("初始化成功！");
				}
				else 
				{
					setTitle("初始化失败");
				}
				
				break;
				
			case R.id.btn_send:
				edit1.setText("Sending ...");
				sendMessage("tst".getBytes(), "tst".getBytes().length);
				edit1.setText("Sent");
				break;
			case R.id.btn_receive:
				text = null;
				edit1.setText("Receiving ...");
				sendMessage("s".getBytes(), "s".getBytes().length);
				
				byte[] tmp = new byte[1024];
				ret = receiveData(tmp,5);
				
				if(ret>0){
					byte[]temp = new byte[ret];
					for(int i=0; i<ret; i++){
						temp[i] = tmp[i];
						text = text + "0x" + Byte.toString(tmp[i])+ " ";
						System.out.println(tmp[i]);
					}
					text = Bytes2HexString(temp);
					edit1.setText(text);
					
				} else if(ret==0){
					System.out.println("接收异常");
					edit1.setText("接收异常");
				} else  //返回 -1
				{
					System.out.println("超时");
					edit1.setText("超时");
				}
				break;
			case R.id.btn_save:
				testSaveData();
				break;
			default:
				break;
			}
		}
	};

	public static String Bytes2HexString(byte[] b) {
		String ret = "";
		for (int i = 0; i < b.length; i++) {
		String hex = "0x" + Integer.toHexString(b[i] & 0xFF) + " ";
		if (hex.length() == 1) {
		hex = '0' + hex;
		}
		ret += hex.toUpperCase();
		}
		return ret;
		} 
	
	public static native int unitInit(int play_rate, int play_channels,
			int rec_rate, int rec_channels, int flags);
	public static native void unitDestroy();
	public static native int receiveData(byte[] recv, int timeout);
	public static native int testSaveData();
	public static native int sendMessage(byte[] str, int length);

	static {
		System.loadLibrary("transfer");
	}
}