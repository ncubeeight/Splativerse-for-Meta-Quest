package com.splativerse.quest;

import android.app.NativeActivity;
import android.app.PendingIntent;
import android.content.Intent;
import android.view.WindowManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;
import org.json.*;
import java.io.File;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public final class ViewerActivity extends NativeActivity {
    private final ExecutorService metadataWorker=LibraryStore.IO;
    private final AtomicBoolean saving=new AtomicBoolean();
    private volatile String updateViewID;
    private float[] entryPose,defaultPose;
    private LibraryStore store;
    // Read once per XR frame; both eyes receive the same status. Android toasts
    // alone are not reliably visible inside an immersive Quest application.
    private int viewHudStatus=1;
    private long viewHudUntil=0,viewHudSince=0;
    private synchronized void viewFeedback(int status,long durationMs){
        viewHudStatus=status;viewHudSince=android.os.SystemClock.elapsedRealtime();viewHudUntil=durationMs==0?Long.MAX_VALUE:viewHudSince+durationMs;
    }
    public synchronized void getViewHudState(float[] state){
        long now=android.os.SystemClock.elapsedRealtime();
        state[0]=now<viewHudUntil?viewHudStatus:1;
        state[1]=state[0]<2?0:Math.min(1f,Math.min((now-viewHudSince)/180f,(viewHudUntil-now)/320f));
        state[2]=(now-viewHudSince)/1000f;
    }
    private static final class ViewListFrame {
        final SavedViewList list;final int page;final int[] atlas;
        ViewListFrame(SavedViewList list,int page,String message){
            this.list=list;this.page=list==null?0:list.page(page);
            String title=list==null?"Saved Views":"Saved Views  ·  "+(this.page+1)+" / "+list.pages();
            this.atlas=ViewCardText.createAtlas(title,list==null?new String[0]:list.names(this.page),message);
        }
        int state(){return list==null?0:list.names(page).length|(page>0?8:0)|(page+1<list.pages()?16:0);}
    }
    private ViewListFrame activeList,loadingList,pendingList;
    private int listGeneration=0,listRevision=0;
    public synchronized int pollViewList(){if(pendingList!=null){activeList=pendingList;pendingList=null;listRevision++;}return listRevision;}
    public synchronized int getViewListState(){return activeList==null?0:activeList.state();}
    public synchronized int[] getViewTextAtlas(){
        if(loadingList==null)loadingList=new ViewListFrame(null,0,"Loading saved Views…");
        return (activeList==null?loadingList:activeList).atlas;
    }
    public synchronized void openViewList(){
        final int generation=++listGeneration;activeList=loadingList;pendingList=null;listRevision++;
        metadataWorker.execute(()->{
            ViewListFrame frame;
            try{frame=new ViewListFrame(new SavedViewList(store.views(getIntent().getStringExtra("digest"))),0,"No saved Views for this scan yet");}
            catch(Exception e){Log.e("Splativerse","Could not load saved Views",e);frame=new ViewListFrame(null,0,"Couldn't load Views. Reopen the list to retry.");}
            synchronized(this){if(generation==listGeneration)pendingList=frame;}
        });
        Log.i("Splativerse","Saved Views list opened");
    }
    public synchronized void closeViewList(){listGeneration++;pendingList=null;}
    // Native thread selects only from the page whose text was uploaded this frame.
    public synchronized float[] viewListAction(int action){
        if(activeList==null||activeList.list==null)return null;
        final SavedViewList list=activeList.list;final int page=activeList.page;
        if(action>=4&&action<=6){
            float[] pose=list.pose(page,action-4);if(pose!=null){closeViewList();viewFeedback(6,4000);Log.i("Splativerse","Immersive View opened: page "+page+" row "+(action-4));}return pose;
        }
        if(action==1||action==3){
            final int next=list.page(page+(action==1?-1:1));if(next==page)return null;
            final int generation=++listGeneration;pendingList=null;
            metadataWorker.execute(()->{ViewListFrame frame=new ViewListFrame(list,next,"");synchronized(this){if(generation==listGeneration)pendingList=frame;}});
        }
        return null;
    }
    public int getMenuHeightStep(){return getSharedPreferences("splativerse",MODE_PRIVATE).getInt("menuHeightStep",0);}
    public void saveMenuHeightStep(int step){getSharedPreferences("splativerse",MODE_PRIVATE).edit().putInt("menuHeightStep",step).apply();}

    // Called by the native thread only after the XR session and graphics resources
    // have been destroyed. Let Horizon Home restore panel input before reopening
    // the library, instead of merely exposing the activity underneath the viewer.
    public void returnToLibraryAfterXrShutdown(){
        runOnUiThread(()->{
            if(isDestroyed())return;
            try {
                Intent panel=new Intent(this,LibraryActivity.class)
                    .setAction(Intent.ACTION_MAIN).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                PendingIntent pending=PendingIntent.getActivity(this,0,panel,
                    PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
                Intent home=new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra("extra_launch_in_home_pending_intent",pending);
                startActivity(home);
                Log.i("Splativerse","Return to library: Home handoff requested after XR shutdown");
            }catch(RuntimeException e){
                Log.e("Splativerse","Could not hand the library back to Home",e);
                Toast.makeText(this,"Please reopen Splativerse from Quest Home.",Toast.LENGTH_LONG).show();
            }finally{Log.i("Splativerse","Viewer task "+getTaskId()+": removing after Home handoff");finishAndRemoveTask();}
        });
    }

    @Override public void onCreate(Bundle state) {
        String error=null;
        try {
            File external=getExternalFilesDir(null);
            store=new LibraryStore(new File(external!=null?external:getFilesDir(),"Library"));
            entryPose=readPose("entryPose");defaultPose=readPose("defaultPose");
            updateViewID=getIntent().getStringExtra("updateViewID");
        } catch(Exception e){error=e.getMessage();}
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if(error!=null){onNativeError(error);finish();}
    }
    private float[] readPose(String key)throws Exception {
        String json=getIntent().getStringExtra(key);if(json==null)return null;
        JSONObject pose=new JSONObject(json);LibraryStore.validatePose(pose);float[] out=new float[6];
        for(int i=0;i<3;i++){out[i]=(float)pose.getJSONArray("position").getDouble(i);out[i+3]=(float)pose.getJSONArray("forward").getDouble(i);}return out;
    }
    public float[] getEntryPose(){return entryPose;}
    public float[] getDefaultPose(){return defaultPose;}
    public boolean getPixelFilter(){return getIntent().getBooleanExtra("pixelFilter",false);}
    public String getScanPath(){return getIntent().getStringExtra("scan");}
    public String getComparePath(){return getIntent().getStringExtra("compareScan");}
    public String getCaptureDirectory(){return new File(getExternalFilesDir(null),"Comparisons").getAbsolutePath();}
    private void notice(String message){Log.i("Splativerse",message);runOnUiThread(()->{if(!isDestroyed())Toast.makeText(this,message,Toast.LENGTH_LONG).show();});}
    public void onComparisonStatus(String message){notice(message);}
    public void onSceneReady(){if(getComparePath()!=null){notice("Left-stick click saves comparison images. X switches detail; Y switches filtering.");return;}if(updateViewID!=null)viewFeedback(7,8000);else if(entryPose!=null)viewFeedback(6,4000);notice(updateViewID==null?"Click the left stick to save a View. B returns to the library.":"Move to the new position, then click the left stick to update this View.");}
    public int getLiveRadius(){return getIntent().getIntExtra("radius",60);}
    private void saveSettings(Integer radius,Boolean pixel){
        if(metadataWorker.isShutdown())return;
        metadataWorker.execute(()->{try{
            String digest=getIntent().getStringExtra("digest");JSONObject metadata=store.metadata(digest);
            if(metadata==null)throw new IllegalStateException("The scan is missing from the library.");
            JSONObject settings=metadata.getJSONObject("settings");int live=radius==null?metadata.optInt("liveRadius",60):radius;
            if(radius!=null&&radius>=60)settings.put("bubbleRadius",radius);
            if(pixel!=null)settings.put("splatFilter",pixel?"pixel":"source");
            store.configure(digest,settings,live,null,false);
        }catch(Exception e){notice("Could not save settings: "+e.getMessage());}});
    }
    public void saveLiveRadius(int radius){saveSettings(radius,null);}
    public void savePixelFilter(boolean pixel){saveSettings(null,pixel);}
    public void saveView(float[] values){
        if(values==null||values.length!=6||metadataWorker.isShutdown()||!saving.compareAndSet(false,true))return;
        final float[] poseValues=values.clone();final String update=updateViewID;
        viewFeedback(2,0);
        notice(update==null?"Saving View…":"Updating View…");
        metadataWorker.execute(()->{try{
            JSONArray position=new JSONArray(),forward=new JSONArray();for(int i=0;i<3;i++){position.put(poseValues[i]);forward.put(poseValues[i+3]);}
            JSONObject pose=new JSONObject().put("position",position).put("forward",forward);
            JSONObject view=store.saveView(new File(getScanPath()),getIntent().getStringExtra("digest"),pose,null,update);
            if(update!=null)updateViewID=null;
            getSharedPreferences("splativerse",MODE_PRIVATE).edit().remove("lastViewError").apply();
            viewFeedback(update==null?3:4,6000);
            notice((update==null?"Saved “":"Updated “")+view.getString("name")+"”. Find it in the library.");
            Log.i("Splativerse","View saved: "+view.toString());
        }catch(Exception e){
            getSharedPreferences("splativerse",MODE_PRIVATE).edit().putString("lastViewError",e.getMessage()==null?"Unknown save error.":e.getMessage()).apply();
            viewFeedback(5,10000);notice("Could not save View: "+e.getMessage());
        }finally{saving.set(false);}});
    }
    public float getUnits(){return getIntent().getFloatExtra("units",1f);}
    public int getUpAxis(){return getIntent().getIntExtra("axis",0);}
    public void onNativeError(String error){
        getSharedPreferences("splativerse",MODE_PRIVATE).edit().putString("lastError",error).apply();
        Log.e("Splativerse",error);
    }
}
