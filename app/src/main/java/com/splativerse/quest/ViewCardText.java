package com.splativerse.quest;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

/** Rasterized once per viewer session; the GPU filters this antialiased alpha atlas. */
public final class ViewCardText {
    public static final int WIDTH=1024, ROW_HEIGHT=256, ROWS=16;
    private ViewCardText(){}
    public static int[] createAtlas(){return createAtlas("Saved Views",new String[0],"Loading saved Views…");}
    public static int[] createAtlas(String listTitle,String[] names,String listHint){
        String[] titles={"Saving view", "View saved", "View updated", "Couldn't save view", "View opened", "Update this view"};
        String[] hints={"Keeping your place…", "Press B to name your View", "Your new position is saved", "Return to library for details", "Back at your saved position", "Move, then click the left stick"};
        Bitmap bitmap=Bitmap.createBitmap(WIDTH,ROW_HEIGHT*ROWS,Bitmap.Config.ARGB_8888);
        try {
            Canvas canvas=new Canvas(bitmap);
            Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);
            paint.setColor(Color.WHITE);
            for(int row=0;row<titles.length;row++){
                paint.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));paint.setTextSize(62);
                fit(paint,titles[row],710);canvas.drawText(titles[row],246,row*ROW_HEIGHT+117,paint);
                paint.setTypeface(Typeface.create("sans-serif",Typeface.NORMAL));paint.setTextSize(31);
                fit(paint,hints[row],710);canvas.drawText(hints[row],246,row*ROW_HEIGHT+169,paint);
            }
            // Two 1024x384 control pages: normal viewing and a paired comparison.
            controls(canvas,paint,1536,false);controls(canvas,paint,1920,true);
            String[] radii={"Full scan", "30 m radius", "60 m radius", "120 m radius", "180 m radius", "240 m radius", "300 m radius", "360 m radius", "Updating radius…"};
            strips(canvas,paint,radii,2304,32,300);
            strips(canvas,paint,new String[]{"Original look","Sharper detail","Smooth edges"},3072,32,270);
            strips(canvas,paint,new String[]{"Explore","Full detail","Reduced copy"},3328,38,340);
            draw(canvas,paint,listTitle,72,3584+78,34,true);
            key(canvas,paint,"X",830,3584+78);draw(canvas,paint,"Close",866,3584+78,27,false);
            for(int i=0;i<names.length;i++){
                paint.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));paint.setTextSize(32);
                String name=android.text.TextUtils.ellipsize(names[i],new android.text.TextPaint(paint),820,android.text.TextUtils.TruncateAt.END).toString();
                canvas.drawText(name,96,3584+139+i*80,paint);
            }
            if(names.length==0){draw(canvas,paint,listHint,96,3584+174,29,false);draw(canvas,paint,"Click the left stick in the scene to save a View",96,3584+225,25,false);}
            draw(canvas,paint,"Point + R trigger to open   ·   X closes list   ·   B library",72,3584+341,23,false);
            int[] pixels=new int[WIDTH*ROW_HEIGHT],packed=new int[WIDTH*ROW_HEIGHT*ROWS/4];
            for(int row=0;row<ROWS;row++){
                bitmap.getPixels(pixels,0,WIDTH,0,row*ROW_HEIGHT,WIDTH,ROW_HEIGHT);
                for(int i=0;i<pixels.length;i++)packed[(row*pixels.length+i)/4]|=(pixels[i]>>>24)<<((i&3)*8);
            }
            return packed;
        } finally {bitmap.recycle();}
    }
    private static void draw(Canvas c,Paint p,String text,float x,float y,float size,boolean medium){
        p.setTypeface(Typeface.create(medium?"sans-serif-medium":"sans-serif",Typeface.NORMAL));p.setTextSize(size);c.drawText(text,x,y,p);
    }
    private static void key(Canvas c,Paint p,String label,float center,float baseline){
        p.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));p.setTextSize(25);c.drawText(label,center-p.measureText(label)/2,baseline,p);
    }
    private static void controls(Canvas c,Paint p,int top,boolean comparison){
        key(c,p,"B",830,top+78);draw(c,p,"Library",866,top+78,27,false);
        key(c,p,"X",395,top+78);draw(c,p,comparison?"Switch detail":"Saved Views",430,top+78,25,false);
        String[] left={"L stick","L click","R click"},right={"R stick","A","Y"};
        for(int i=0;i<3;i++){key(c,p,left[i],130,top+135+i*80);key(c,p,right[i],596,top+135+i*80);}
        draw(c,p,"Move / strafe",216,top+139,32,false);draw(c,p,"Turn / height",682,top+139,32,false);
        draw(c,p,comparison?"Save images":"Save View",216,top+219,32,false);draw(c,p,"Return to start",682,top+219,32,false);
        draw(c,p,"Point + R trigger to select   ·   L trigger shows menu",72,top+341,23,false);
    }
    private static void strips(Canvas c,Paint p,String[] labels,int top,float size,float width){
        for(int i=0;i<labels.length;i++){
            p.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));p.setTextSize(size);fit(p,labels[i],width);c.drawText(labels[i],0,top+i*64+45,p);
        }
    }
    private static void fit(Paint paint,String text,float width){
        float measured=paint.measureText(text);if(measured>width)paint.setTextSize(paint.getTextSize()*width/measured);
    }
}
