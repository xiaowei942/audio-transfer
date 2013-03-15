package com.thinpad.audiotransfer;

public class AnalysisData implements Runnable {

	@Override
	public void run() {
		// TODO Auto-generated method stub
		testReadFile();
	}

	public static native int testReadFile();
	
	static {
		System.loadLibrary("transfer");
	}
}
