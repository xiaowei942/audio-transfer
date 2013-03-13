package com.thinpad.audiotransfer;

public class AnalysisData implements Runnable {

	@Override
	public void run() {
		// TODO Auto-generated method stub
		testReadData();
	}

	public static native int testReadData();
	
	static {
		System.loadLibrary("transfer");
	}
}
