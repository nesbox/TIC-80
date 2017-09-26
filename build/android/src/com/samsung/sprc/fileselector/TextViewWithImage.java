package com.samsung.sprc.fileselector;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Class which combines ImageView and TextView in LineralLayout with horizontal
 * orientation
 */
public class TextViewWithImage extends LinearLayout {

	/**
	 * Image - in this project will be used to display icon representing the
	 * file type
	 */
	private ImageView mImage;
	/** Text - in this project will be used to display the file name */
	private TextView mText;

	public TextViewWithImage(Context context) {
		super(context);
		setOrientation(HORIZONTAL);
		mImage = new ImageView(context);
		mText = new TextView(context);

		LayoutParams lp = new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1);
		lp.weight = 1;
		addView(mImage, lp);
		lp = new LayoutParams(0, LayoutParams.WRAP_CONTENT, 3);
		addView(mText, lp);
	}

	/** Simple wrapper around the TextView.getText() method. */
	public CharSequence getText() {
		return mText.getText();
	}

	/**
	 * Simple wrapper around ImageView.setImageResource() method. but if resId
	 * is equal -1 this method sets Images visibility as GONE
	 */
	public void setImageResource(int resId) {
		if (resId == -1) {
			mImage.setVisibility(View.GONE);
			return;
		}
		mImage.setImageResource(resId);
	}

	/** Simple wrapper around TextView.setText() method. */
	public void setText(String aText) {
		mText.setText(aText);
	}

}
