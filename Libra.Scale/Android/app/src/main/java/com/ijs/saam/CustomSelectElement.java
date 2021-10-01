package com.ijs.saam;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.util.LayoutDirection;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.Space;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;

import com.ijs.saam.msg.ActivityMsg;
import com.ijs.saam.msg.CustomMsg;
import com.ijs.saam.utils.ACTIVITY_VAR;

import org.greenrobot.eventbus.EventBus;

public class CustomSelectElement extends LinearLayout {
    private static final String TAG = "CustomSelectElement";

    private LinearLayout llParent;
    private ImageView image;
    private TextView text;
    private String name;
    private boolean selected = false;

    public CustomSelectElement(final Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        TypedArray ta = context.obtainStyledAttributes(attrs, R.styleable.CustomSelectElement, 0, 0);

        // Define
        image = new ImageView(context);
        text = new TextView(context);
        llParent = this;

        // Edit
        // layout
        this.setGravity(Gravity.CENTER);
        this.setOrientation(LinearLayout.VERTICAL);

        // image
        image.setImageResource(ta.getResourceId(R.styleable.CustomSelectElement_customImage, 0));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            lp.setMargins(0, 0, 0, 10);
        image.setLayoutParams(lp);

        // text
        text.setText(ta.getText(R.styleable.CustomSelectElement_customText));
        text.setTextColor(ContextCompat.getColor(context, R.color.fontColor));
        text.setTypeface(ResourcesCompat.getFont(context, R.font.opensanssemibold));
        text.setGravity(Gravity.CENTER);

        // name
        name = ta.getString(R.styleable.CustomSelectElement_name);

        // Set to view
        addView(image);
        addView(text);
        
        this.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if(!selected) {
                    // select
                    selected = true;
                    llParent.setBackground(ContextCompat.getDrawable(context, R.drawable.custom_contains_background));
                } else {
                    // deselect
                    selected = false;
                    llParent.setBackground(null);
                }
                EventBus.getDefault().post(new CustomMsg(ACTIVITY_VAR.REFRESH_CONTENT));
            }
        });
        ta.recycle();
    }

    /**
     * Function for obtaining the name of the custom element.
     * @return String value of the name of the element
     */
    public String getName() {return name;}

    /**
     * Function for obtaining information if the custom element is selected or not.
     * @return true if the element is selected and false otherwise.
     */
    public boolean isSelected() {return selected;}

    /**
     * Function for setting element select
     * @param selected true or false
     */
    public void setSelected(boolean selected) {
        this.selected = selected;
        if(selected) {
            // select
            llParent.setBackground(ContextCompat.getDrawable(getContext(), R.drawable.custom_contains_background));
        } else {
            // deselect
            llParent.setBackground(null);
        }
    }
}
