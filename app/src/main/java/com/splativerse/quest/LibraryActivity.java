package com.splativerse.quest;

import android.app.*;
import android.content.*;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.text.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.text.DateFormat;
import java.util.*;
import java.util.concurrent.*;

public final class LibraryActivity extends Activity {
    private static final String FREE_SCANS_URL = "https://splativerse.app/";
    private final ExecutorService worker=LibraryStore.IO;
    private final int ink=Color.rgb(238,241,239),muted=Color.rgb(165,175,172),green=Color.rgb(116,226,180),surface=Color.rgb(34,39,37);
    private SharedPreferences preferences;private LibraryStore store;private File directory;
    private final List<LibraryStore.Item> items=new ArrayList<>(),visible=new ArrayList<>();
    private LinearLayout root,body,sidebar,detail;private TextView status,summary;private Button importButton,refreshButton,browseButton;private ListView list;private ScanAdapter adapter;
    private Set<String> hiddenScans=new HashSet<>();private boolean showHidden=false;
    private String selected="",query="",format="All formats";private int sort=0;private boolean busy=true,wide=false,detailVisible=false;
    private int dp(float v){return Math.round(v*getResources().getDisplayMetrics().density);}
    private GradientDrawable bg(int color){GradientDrawable b=new GradientDrawable();b.setColor(color);b.setCornerRadius(dp(12));return b;}
    private TextView text(String value,int size,int color){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);t.setPadding(0,dp(5),0,dp(5));return t;}
    private TextView heading(String value,int size){TextView t=text(value,size,ink);t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);return t;}
    private LinearLayout column(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.VERTICAL);return l;}
    private LinearLayout row(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.HORIZONTAL);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    private Button button(String label,boolean primary,Runnable action){Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setTextSize(15);b.setTextColor(primary?Color.rgb(13,35,24):ink);b.setBackground(bg(primary?green:Color.rgb(48,55,51)));b.setMinimumHeight(dp(48));b.setPadding(dp(16),0,dp(16),0);b.setOnClickListener(v->{if(!busy)action.run();});LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(48));p.setMargins(0,dp(6),0,dp(6));b.setLayoutParams(p);return b;}
    private void addAction(LinearLayout row,Button b){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(48),1);p.setMargins(dp(3),dp(6),dp(3),dp(6));row.addView(b,p);}
    private void enabled(Button b,boolean value){b.setEnabled(value);b.setAlpha(value?1f:0.4f);}
    private void ui(Runnable action){runOnUiThread(()->{if(!isDestroyed())action.run();});}
    private void message(String value,boolean error){status.setText(value);status.setTextColor(error?Color.rgb(255,187,157):muted);}
    private void busy(boolean value,String label){busy=value;enabled(importButton,!value);enabled(refreshButton,!value);enabled(browseButton,!value);if(label!=null)message(label,false);if(adapter!=null)adapter.notifyDataSetChanged();}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);android.util.Log.i("Splativerse","Panel created: task "+getTaskId());preferences=getSharedPreferences("splativerse",MODE_PRIVATE);selected=preferences.getString("selectedDigest","");hiddenScans=new HashSet<>(preferences.getStringSet("hiddenScanDigests",Collections.emptySet()));
        File external=getExternalFilesDir(null),base=external!=null?external:getFilesDir();directory=new File(base,"Scans");
        wide=getResources().getDisplayMetrics().widthPixels/getResources().getDisplayMetrics().density>=800;
        root=column();root.setPadding(dp(22),dp(16),dp(22),dp(16));root.setBackgroundColor(Color.rgb(19,23,21));setContentView(root);
        LinearLayout toolbar=row();LinearLayout brand=column();brand.addView(heading("Splativerse",27));brand.addView(text("YOUR SCANS & SAVED VIEWS",11,green));toolbar.addView(brand,new LinearLayout.LayoutParams(0,-2,1));
        importButton=button("Import scans",true,this::pick);toolbar.addView(importButton,new LinearLayout.LayoutParams(dp(140),dp(48)));
        refreshButton=button("Refresh",false,this::loadLibrary);LinearLayout.LayoutParams rp=new LinearLayout.LayoutParams(dp(100),dp(48));rp.setMargins(dp(8),0,0,0);toolbar.addView(refreshButton,rp);
        Button help=button("Help",false,this::help);LinearLayout.LayoutParams hp=new LinearLayout.LayoutParams(dp(74),dp(48));hp.setMargins(dp(8),0,0,0);toolbar.addView(help,hp);root.addView(toolbar);
        status=text("Preparing your library…",13,muted);status.setMinHeight(dp(35));root.addView(status);
        body=row();body.setGravity(Gravity.TOP);root.addView(body,new LinearLayout.LayoutParams(-1,0,1));
        sidebar=column();sidebar.setPadding(dp(12),dp(8),dp(12),dp(8));sidebar.setBackground(bg(surface));body.addView(sidebar,new LinearLayout.LayoutParams(wide?dp(310):-1,-1));
        browseButton = button("Browse free scans", false, this::browseFreeScans);
        sidebar.addView(browseButton);
        EditText search=new EditText(this);search.setSingleLine(true);search.setHint("Search scans or Views");search.setTextColor(ink);search.setHintTextColor(muted);search.setTextSize(16);sidebar.addView(search,new LinearLayout.LayoutParams(-1,dp(50)));
        search.addTextChangedListener(new TextWatcher(){public void beforeTextChanged(CharSequence s,int st,int c,int a){}public void onTextChanged(CharSequence s,int st,int before,int count){query=s.toString();filter();}public void afterTextChanged(Editable e){}});
        Spinner order=spinner(new String[]{"Name A–Z","Recently opened","Largest files","Most saved Views"});sidebar.addView(order);order.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onItemSelected(AdapterView<?> p,View v,int n,long id){sort=n;filter();}public void onNothingSelected(AdapterView<?>p){}});
        Spinner formats=spinner(new String[]{"All formats","SPZ","PLY","SPLAT"});sidebar.addView(formats);formats.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onItemSelected(AdapterView<?>p,View v,int n,long id){format=new String[]{"All formats","SPZ","PLY","SPLAT"}[n];filter();}public void onNothingSelected(AdapterView<?>p){}});
        CheckBox hiddenToggle=new CheckBox(this);hiddenToggle.setText("Show hidden scans");hiddenToggle.setTextColor(muted);hiddenToggle.setTextSize(14);hiddenToggle.setMinHeight(dp(48));sidebar.addView(hiddenToggle);hiddenToggle.setOnCheckedChangeListener((b,checked)->{showHidden=checked;filter();renderDetail();});
        summary=text("",12,muted);sidebar.addView(summary);list=new ListView(this);list.setDivider(null);list.setDividerHeight(dp(6));adapter=new ScanAdapter();list.setAdapter(adapter);sidebar.addView(list,new LinearLayout.LayoutParams(-1,0,1));
        list.setOnItemClickListener((p,v,n,id)->{if(busy)return;selected=visible.get(n).digest;preferences.edit().putString("selectedDigest",selected).apply();detailVisible=true;adapter.notifyDataSetChanged();renderDetail();});
        detail=column();LinearLayout.LayoutParams d=new LinearLayout.LayoutParams(wide?0:-1,-1,wide?1:0);d.setMargins(wide?dp(18):0,0,0,0);body.addView(detail,d);renderDetail();
        busy(true,null);
        worker.execute(()->{try{if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Could not open the scans folder.");store=new LibraryStore(new File(base,"Library"));File demo=new File(directory,"Demo.ply");if(!demo.exists()){File tmp=File.createTempFile("demo-",".partial",directory);try{try(InputStream in=getAssets().open("Demo.ply");OutputStream out=new FileOutputStream(tmp)){copy(in,out);}Files.move(tmp.toPath(),demo.toPath());}finally{tmp.delete();}}ui(()->{busy(false,null);loadLibrary();});}catch(Exception e){failure(e);}});
    }
    @Override public void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);if(!busy)openRequested();}
    @Override public void onResume(){super.onResume();if(store!=null&&!busy)loadLibrary();}
    private Spinner spinner(String[] values){Spinner s=new Spinner(this);ArrayAdapter<String>a=new ArrayAdapter<>(this,android.R.layout.simple_spinner_item,values);a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);s.setAdapter(a);s.setMinimumHeight(dp(46));return s;}
    private void loadLibrary(){
        if(busy||store==null)return;busy(true,"Reading scan details and saved Views…");worker.execute(()->{try{
            File[] files=directory.listFiles((f,n)->LibraryStore.supported(n));if(files==null)throw new IOException("Could not read the scans folder.");Arrays.sort(files,(a,b)->LibraryStore.naturalCompare(a.getName(),b.getName()));
            Map<String,LibraryStore.Item> unique=new LinkedHashMap<>();int read=0;
            for(File f:files){if(Thread.currentThread().isInterrupted())return;if(!f.isFile()||!f.getCanonicalFile().getParentFile().equals(directory.getCanonicalFile()))continue;
                int n=++read;ui(()->message("Reading scan "+n+" of "+files.length+"…",false));LibraryStore.Item item=LibraryStore.inspect(f);
                JSONObject settings=LibraryStore.defaultSettings();int unit=preferences.getInt("units:"+f.getName(),0),axis=preferences.getInt("axis:"+f.getName(),0),radius=preferences.getInt("radius:"+f.getName(),60);
                if(unit<0||unit>2)unit=0;if(axis<0||axis>2)axis=0;try{LibraryStore.checkedRadius(radius);}catch(Exception e){radius=60;}
                settings.put("metresPerUnit",new double[]{1,.01,.001}[unit]).put("upAxis",new String[]{"yDown","yUp","zUp"}[axis]).put("bubbleRadius",Math.max(60,radius));
                String oldTitle=preferences.getString("name:"+f.getName(),null);store.decorate(item,settings,radius,oldTitle!=null&&oldTitle.codePointCount(0,oldTitle.length())<=80?oldTitle:null);
                unique.put(item.digest,unique.containsKey(item.digest)?LibraryStore.mergeCopies(unique.get(item.digest),item):item);
            }
            ui(()->{items.clear();items.addAll(unique.values());if(find(selected)==null)selected=items.isEmpty()?"":items.get(0).digest;busy(false,null);filter();renderDetail();String error=preferences.getString("lastError",null);if(error!=null)message("Could not open the scan: "+error,true);else if(preferences.contains("lastViewError"))message("Could not save View: "+preferences.getString("lastViewError",""),true);else message("Ready · "+items.size()+" scans · original files preserved",false);openRequested();});
        }catch(Exception e){failure(e);}});
    }
    private LibraryStore.Item find(String digest){for(LibraryStore.Item i:items)if(i.digest.equals(digest))return i;return null;}
    private void filter(){
        if(adapter==null)return;visible.clear();String q=query.trim().toLowerCase(Locale.ROOT);
        for(LibraryStore.Item i:items){boolean matches=(i.title+" "+i.filenames.toString()).toLowerCase(Locale.ROOT).contains(q);JSONArray views=i.views.optJSONArray("views");for(int n=0;!matches&&n<views.length();n++)matches=views.optJSONObject(n).optString("name").toLowerCase(Locale.ROOT).contains(q);if(matches&&(format.equals("All formats")||format.equals(i.format))&&(showHidden||!hiddenScans.contains(i.digest)))visible.add(i);}
        visible.sort((a,b)->{int c=sort==1?Long.compare(b.lastOpened,a.lastOpened):sort==2?Long.compare(b.bytes,a.bytes):sort==3?Integer.compare(b.viewCount(),a.viewCount()):0;return c!=0?c:LibraryStore.naturalCompare(a.title,b.title);});
        if(!showHidden&&hiddenScans.contains(selected)){selected=visible.isEmpty()?"":visible.get(0).digest;preferences.edit().putString("selectedDigest",selected).apply();renderDetail();}
        int hiddenCount=0;for(LibraryStore.Item i:items)if(hiddenScans.contains(i.digest))hiddenCount++;
        summary.setText(visible.isEmpty()?(items.isEmpty()?"Import a scan to get started.":hiddenCount==items.size()&&!showHidden?"All scans hidden. Turn on Show hidden scans to restore them.":"No matching scans or Views."):visible.size()+" shown · "+hiddenCount+" hidden");adapter.notifyDataSetChanged();
    }
    private final class ScanAdapter extends BaseAdapter {
        public int getCount(){return visible.size();}public Object getItem(int p){return visible.get(p);}public long getItemId(int p){return p;}
        public View getView(int position,View recycled,android.view.ViewGroup parent){
            LibraryStore.Item i=visible.get(position);boolean hidden=hiddenScans.contains(i.digest);
            LinearLayout card=row();card.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);card.setPadding(dp(12),dp(10),dp(8),dp(10));card.setBackground(bg(i.digest.equals(selected)?Color.rgb(43,71,57):Color.rgb(43,48,45)));
            LinearLayout info=column();TextView title=heading(i.title,16);title.setMaxLines(2);info.addView(title);info.addView(text(i.format+" · "+size(i.bytes)+" · "+i.viewCount()+" Views",12,muted));
            if(hidden)info.addView(text("Hidden from navigation",12,green));if(!i.issue.isEmpty())info.addView(text("Needs attention",12,Color.rgb(255,187,157)));
            card.addView(info,new LinearLayout.LayoutParams(0,-2,1));
            Button hide=button(hidden?"↶":"×",false,()->setScanHidden(i,!hidden));hide.setTextSize(26);hide.setPadding(0,0,0,0);hide.setMinWidth(0);hide.setMinimumWidth(0);hide.setFocusable(false);hide.setContentDescription((hidden?"Restore ":"Hide ")+i.title);hide.setTooltipText(hidden?"Restore to navigation":"Hide from navigation; keep file and Views");enabled(hide,!busy);
            LinearLayout.LayoutParams hp=new LinearLayout.LayoutParams(dp(48),dp(48));hp.setMargins(dp(6),0,0,0);card.addView(hide,hp);return card;
        }
    }
    private void setScanHidden(LibraryStore.Item item,boolean hide){
        if(hide)hiddenScans.add(item.digest);else hiddenScans.remove(item.digest);
        preferences.edit().putStringSet("hiddenScanDigests",new HashSet<>(hiddenScans)).apply();
        filter();renderDetail();message((hide?"Hidden “":"Restored “")+item.title+"”. Scan file and saved Views kept.",false);
        android.util.Log.i("Splativerse","Library visibility: hidden="+hide+"; hidden count="+hiddenScans.size());
    }
    private static String size(long bytes){return String.format(Locale.US,"%.1f MiB",bytes/1048576.0);}
    private void renderDetail(){
        if(detail==null)return;detail.removeAllViews();sidebar.setVisibility(!wide&&detailVisible?View.GONE:View.VISIBLE);detail.setVisibility(wide||detailVisible?View.VISIBLE:View.GONE);
        if(!wide){Button back=button("‹  Back to library",false,()->{detailVisible=false;renderDetail();});detail.addView(back);}
        ScrollView scroll=new ScrollView(this);LinearLayout content=column();content.setPadding(dp(8),0,dp(8),dp(16));scroll.addView(content);detail.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));LibraryStore.Item i=find(selected);
        if(i==null){content.addView(heading("Your next space",26));content.addView(text("Import a PLY, SPZ or SPLAT scan, then save the places you want to return to.",17,muted));return;}
        content.addView(heading(i.title,26));content.addView(text(i.format+"   ·   "+size(i.bytes)+"   ·   "+(i.count>0?String.format(Locale.US,"%,d splats",i.count):"Count unavailable"),15,green));
        content.addView(text("File: "+i.file.getName()+(i.copies>1?" · "+i.copies+" identical copies":"")+"\nModified "+DateFormat.getDateTimeInstance(DateFormat.MEDIUM,DateFormat.SHORT).format(new Date(i.modified)),12,muted));
        if(!i.issue.isEmpty())content.addView(text(i.issue,15,Color.rgb(255,187,157)));
        LinearLayout actions=row();Button enter=button("Enter this space",true,()->launch(i,null,null,null));enabled(enter,i.canOpen());addAction(actions,enter);addAction(actions,button("Rename",false,()->editName("Rename scan",i.title,value->mutate(()->store.configure(i.digest,i.settings,i.radius,value,false)))));content.addView(actions);
        String mode=i.settings.optString("splatFilter","source").equals("pixel")?"Sharper detail":"Original scan appearance";
        content.addView(button((i.radius==0?"Full scan":i.radius+" m radius")+" · "+mode+"   ▾",false,()->settings(i)));
        content.addView(heading("Saved Views",21));content.addView(text("Click the left stick while exploring to save your position and heading. Name and organise your Views here.",14,muted));
        JSONArray views=i.views.optJSONArray("views");String defaultID=i.views.optString("defaultID","");
        if(views.length()==0){LinearLayout empty=column();empty.setBackground(bg(surface));empty.setPadding(dp(18),dp(12),dp(18),dp(12));empty.addView(text("No saved Views yet",17,ink));empty.addView(text("Enter this space and find your first viewpoint.",14,muted));content.addView(empty);}
        for(int n=0;n<views.length();n++){JSONObject view=views.optJSONObject(n);String id=view.optString("id"),name=view.optString("name");LinearLayout card=column();card.setBackground(bg(surface));card.setPadding(dp(14),dp(9),dp(14),dp(9));LinearLayout.LayoutParams cp=new LinearLayout.LayoutParams(-1,-2);cp.setMargins(0,dp(5),0,dp(5));content.addView(card,cp);
            card.addView(heading(name,18));if(id.equalsIgnoreCase(defaultID))card.addView(text("★ Starting View",12,green));
            LinearLayout vr=row();Button go=button("Go to View",true,()->launch(i,view.optJSONObject("pose"),null,null));enabled(go,i.canOpen());addAction(vr,go);Button more=button("Manage View",false,()->viewMenu(i,view));addAction(vr,more);card.addView(vr);
        }
        if(!defaultID.isEmpty())content.addView(button("Use automatic starting point",false,()->mutate(()->store.editView(i.digest,"","automatic",null))));
        else content.addView(text("Starting point: automatic dense region",12,muted));
        content.addView(text("Views follow this scan’s content, so renaming or reimporting the same file keeps them together.",12,muted));
        content.addView(button("Compare a reduced copy…",false,()->compare(i)));
    }
    private interface Change {void run()throws Exception;}private interface Named {void set(String value)throws Exception;}
    private void mutate(Change change){busy(true,"Saving…");worker.execute(()->{try{change.run();ui(()->{busy(false,null);loadLibrary();});}catch(Exception e){failure(e);}});}
    private void editName(String title,String value,Named action){EditText name=new EditText(this);name.setSingleLine(true);name.setText(value);name.setSelectAllOnFocus(true);name.setPadding(dp(20),dp(12),dp(20),dp(12));AlertDialog dialog=new AlertDialog.Builder(this).setTitle(title).setView(name).setNegativeButton("Cancel",null).setPositiveButton("Save",null).create();dialog.setOnShowListener(d->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{String n=name.getText().toString().trim();if(n.isEmpty()||n.codePointCount(0,n.length())>80){name.setError("Use 1–80 characters");return;}try{action.set(n);dialog.dismiss();}catch(Exception e){name.setError(e.getMessage());}}));dialog.show();}
    private void viewMenu(LibraryStore.Item item,JSONObject view){String id=view.optString("id");String[] choices={"Rename","Use as starting View","Update position in headset…","Delete View…"};new AlertDialog.Builder(this).setTitle(view.optString("name")).setItems(choices,(d,n)->{
        if(n==0)editName("Rename View",view.optString("name"),name->mutate(()->store.editView(item.digest,id,"rename",name)));
        if(n==1)mutate(()->store.editView(item.digest,id,"default",null));
        if(n==2)launch(item,view.optJSONObject("pose"),id,null);
        if(n==3)new AlertDialog.Builder(this).setTitle("Delete this View?").setMessage("Delete “"+view.optString("name")+"”? The scan itself is kept.").setNegativeButton("Cancel",null).setPositiveButton("Delete",(a,b)->mutate(()->store.editView(item.digest,id,"delete",null))).show();
    }).show();}
    private void settings(LibraryStore.Item i){try{
        LinearLayout panel=column();panel.setPadding(dp(22),dp(8),dp(22),dp(8));panel.addView(text("Live radius",14,ink));int[] radii={30,60,120,180,240,300,360,0};Spinner radius=spinner(new String[]{"30 m","60 m","120 m","180 m","240 m","300 m","360 m","Full scan"});for(int n=0;n<radii.length;n++)if(i.radius==radii[n])radius.setSelection(n);panel.addView(radius);panel.addView(text("Smaller radii show less distant scenery. The outer 5 m fades into grey.",12,muted));
        panel.addView(text("Live filter",14,ink));Spinner filter=spinner(new String[]{"Original scan appearance","Sharper detail (experimental)"});filter.setSelection(i.settings.getString("splatFilter").equals("pixel")?1:0);panel.addView(filter);
        panel.addView(text("Source units",14,ink));double currentUnits=i.settings.getDouble("metresPerUnit");Spinner units=spinner(new String[]{"Metres","Centimetres","Millimetres","Keep current scale ("+currentUnits+")"});units.setSelection(currentUnits==1?0:currentUnits==.01?1:currentUnits==.001?2:3);panel.addView(units);
        panel.addView(text("Up direction",14,ink));Spinner axis=spinner(new String[]{"Y down (3DGS / SPZ)","Y up","Z up"});axis.setSelection(Arrays.asList("yDown","yUp","zUp").indexOf(i.settings.getString("upAxis")));panel.addView(axis);
        ScrollView scroll=new ScrollView(this);scroll.addView(panel);new AlertDialog.Builder(this).setTitle("Scan settings").setView(scroll).setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{try{JSONObject s=new JSONObject(i.settings.toString());int r=radii[radius.getSelectedItemPosition()];if(r>=60)s.put("bubbleRadius",r);s.put("splatFilter",filter.getSelectedItemPosition()==1?"pixel":"source");s.put("metresPerUnit",units.getSelectedItemPosition()<3?new double[]{1,.01,.001}[units.getSelectedItemPosition()]:currentUnits);s.put("upAxis",new String[]{"yDown","yUp","zUp"}[axis.getSelectedItemPosition()]);mutate(()->store.configure(i.digest,s,r,null,false));}catch(Exception e){failure(e);}}).show();
    }catch(Exception e){failure(e);}}
    private void launch(LibraryStore.Item item,JSONObject pose,String updateID,LibraryStore.Item comparison){
        if(busy||!item.canOpen())return;busy(true,"Verifying scan and preparing your View…");worker.execute(()->{try{LibraryStore.verify(item.file,item.digest);if(comparison!=null)LibraryStore.verify(comparison.file,comparison.digest);JSONObject views=store.views(item.digest),start=LibraryStore.startingPose(views);JSONObject entry=pose==null?start:pose;if(entry!=null)LibraryStore.validatePose(entry);
            store.configure(item.digest,item.settings,item.radius,null,true);Intent intent=new Intent(this,ViewerActivity.class).setAction(Intent.ACTION_MAIN).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);intent.putExtra("scan",item.file.getAbsolutePath());intent.putExtra("digest",item.digest);intent.putExtra("units",(float)item.settings.getDouble("metresPerUnit"));intent.putExtra("axis",Arrays.asList("yDown","yUp","zUp").indexOf(item.settings.getString("upAxis")));intent.putExtra("radius",item.radius);intent.putExtra("pixelFilter",item.settings.getString("splatFilter").equals("pixel"));
            if(entry!=null)intent.putExtra("entryPose",entry.toString());if(start!=null)intent.putExtra("defaultPose",start.toString());if(updateID!=null)intent.putExtra("updateViewID",updateID);if(comparison!=null)intent.putExtra("compareScan",comparison.file.getAbsolutePath());
            preferences.edit().putString("selectedDigest",item.digest).remove("lastError").apply();ui(()->{busy(false,null);detailVisible=true;startActivity(intent);android.util.Log.i("Splativerse","Panel task "+getTaskId()+": removing after immersive launch");finishAndRemoveTask();});
        }catch(Exception e){failure(e);}});
    }
    private void compare(LibraryStore.Item item){List<LibraryStore.Item> options=new ArrayList<>();for(LibraryStore.Item i:items)if(!i.digest.equals(item.digest)&&i.canOpen()&&i.count<item.count)options.add(i);if(options.isEmpty()){message("Import an unchanged reduced copy of this scan to compare detail.",false);return;}String[] names=new String[options.size()];for(int i=0;i<names.length;i++)names[i]=options.get(i).title;new AlertDialog.Builder(this).setTitle("Choose the reduced copy").setItems(names,(d,n)->new AlertDialog.Builder(this).setTitle("Compare detail").setMessage("X switches full/reduced detail. Y switches filtering. Left-stick click saves four matched images in comparison mode.").setNegativeButton("Cancel",null).setPositiveButton("Compare",(a,b)->launch(item,null,null,options.get(n))).show()).show();}
    private void openRequested(){
        if(busy)return;String name=getIntent().getStringExtra("scanName");if(getIntent().getBooleanExtra("demo",false))name="Demo.ply";if(name==null)return;getIntent().removeExtra("scanName");getIntent().removeExtra("demo");
        for(LibraryStore.Item i:items)if(i.filenames.contains(name)){selected=i.digest;detailVisible=true;String id=getIntent().getStringExtra("viewId");getIntent().removeExtra("viewId");JSONObject pose=null;JSONArray views=i.views.optJSONArray("views");if(id!=null){for(int n=0;n<views.length();n++)if(views.optJSONObject(n).optString("id").equalsIgnoreCase(id))pose=views.optJSONObject(n).optJSONObject("pose");if(pose==null){message("The requested saved View was not found.",true);renderDetail();return;}}
            if(getIntent().hasExtra("radius")){int radius=getIntent().getIntExtra("radius",60);getIntent().removeExtra("radius");try{i.radius=LibraryStore.checkedRadius(radius);}catch(Exception ignored){}}
            String compareName=getIntent().getStringExtra("compareName");getIntent().removeExtra("compareName");LibraryStore.Item reduced=null;for(LibraryStore.Item candidate:items)if(candidate.filenames.contains(compareName))reduced=candidate;renderDetail();launch(i,pose,null,reduced);return;}
        message("The requested scan was not found. Refresh the library.",true);
    }
    private void browseFreeScans() {
        Intent browser = new Intent(Intent.ACTION_VIEW, Uri.parse(FREE_SCANS_URL));
        try {
            startActivity(browser);
        } catch (ActivityNotFoundException e) {
            new AlertDialog.Builder(this)
                .setTitle("Browse free scans")
                .setMessage("Open " + FREE_SCANS_URL + " in Quest Browser. After downloading an SPZ scan, return here and choose Import scans → Downloads.")
                .setPositiveButton("Done", null)
                .show();
        }
    }

    private void pick(){Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);i.addCategory(Intent.CATEGORY_OPENABLE);i.setType("*/*");i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE,true);try{startActivityForResult(i,42);}catch(Exception e){message("No file picker is installed. Scans can also be copied to the app’s Scans folder over USB.",true);}}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request!=42||result!=RESULT_OK||data==null)return;List<Uri> uris=new ArrayList<>();if(data.getClipData()!=null)for(int i=0;i<data.getClipData().getItemCount();i++)uris.add(data.getClipData().getItemAt(i).getUri());else if(data.getData()!=null)uris.add(data.getData());if(uris.isEmpty())return;
        busy(true,"Importing scans…");Set<String> known=new HashSet<>();for(LibraryStore.Item item:items)known.add(item.digest);worker.execute(()->{List<String> errors=new ArrayList<>();int imported=0,duplicates=0;
            for(Uri uri:uris){File tmp=null;try{String name="scan";try(Cursor c=getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){if(c!=null&&c.moveToFirst())name=c.getString(0);}if(name==null||!LibraryStore.supported(name))throw new IOException("Choose PLY, SPZ or SPLAT scans. Package sharing will be added separately.");tmp=File.createTempFile("import-",".partial",directory);MessageDigest hash=MessageDigest.getInstance("SHA-256");long bytes=0;try(InputStream in=getContentResolver().openInputStream(uri);FileOutputStream out=new FileOutputStream(tmp)){if(in==null)throw new IOException("Could not open "+name);byte[] block=new byte[1024*1024];int n;while((n=in.read(block))!=-1){bytes+=n;if(bytes>LibraryStore.MAX_FILE)throw new IOException(name+" exceeds 128 MiB.");hash.update(block,0,n);out.write(block,0,n);}out.getFD().sync();}if(bytes==0)throw new IOException(name+" is empty.");StringBuilder digest=new StringBuilder();for(byte b:hash.digest())digest.append(String.format(Locale.ROOT,"%02x",b&255));String sha=digest.toString();if(known.contains(sha)){duplicates++;tmp.delete();continue;}
                String ext=name.substring(name.lastIndexOf('.')).toLowerCase(Locale.ROOT);File destination=new File(directory,sha+ext);if(destination.exists()){if(!LibraryStore.fingerprint(destination).equals(sha))throw new IOException("A conflicting file is already present. Refresh the library.");duplicates++;}else{Files.move(tmp.toPath(),destination.toPath());imported++;}known.add(sha);String title=name.codePointCount(0,name.length())<=80?name:null;if(store.metadata(sha)==null)store.configure(sha,LibraryStore.defaultSettings(),60,title,false);selected=sha;
            }catch(Exception e){errors.add(e.getMessage()==null?"Import failed.":e.getMessage());}finally{if(tmp!=null)tmp.delete();}}
            final int added=imported,existing=duplicates;ui(()->{busy(false,null);loadLibrary();Toast.makeText(this,added+" imported · "+existing+" already in library",Toast.LENGTH_LONG).show();if(!errors.isEmpty())new AlertDialog.Builder(this).setTitle("Some files could not be imported").setMessage(android.text.TextUtils.join("\n\n",errors)).setPositiveButton("OK",null).show();});
        });
    }
    private void help(){new AlertDialog.Builder(this).setTitle("Exploring and saving Views").setMessage("Choose Browse free scans to open Splativerse.app in your browser. After downloading an SPZ scan, return here and choose Import scans → Downloads.\n\nLeft stick: move / strafe\nRight stick: snap turn / move up and down\nLeft-stick click: save a View\nRight-stick click: change radius\nX: open or close this scan’s saved Views\nY: switch between original scan appearance and sharper detail\nA: return to your starting View\nB: return to this library\nRight trigger: select menu arrows or X\nLeft trigger: show the hidden controls menu\n\nUse × beside a library scan to hide it without deleting its file or saved Views. Turn on Show hidden scans and choose ↶ to restore it.\n\n“Smooth edges” means the scan’s built-in smoothing is active. “Original look” preserves a scan without that smoothing. “Sharper detail” uses an alternative sharpening filter.\n\nIn a detail comparison, X switches detail and left-stick click saves comparison images.\n\nViews are named automatically. Rename, update, delete or choose a starting View in the library.\n\nQuest preview 0.1.14 · Up to 1,000,000 splats / 128 MiB. Movement has no collision detection.").setNegativeButton("Licenses",(d,w)->licenses()).setPositiveButton("Done",null).show();}
    private void licenses(){try(InputStream in=getAssets().open("THIRD_PARTY_LICENSES.txt")){ByteArrayOutputStream out=new ByteArrayOutputStream();copy(in,out);ScrollView scroll=new ScrollView(this);TextView t=text(out.toString("UTF-8"),13,ink);t.setPadding(dp(20),dp(12),dp(20),dp(12));scroll.addView(t);new AlertDialog.Builder(this).setTitle("Open-source licenses").setView(scroll).setPositiveButton("Close",null).show();}catch(Exception e){failure(e);}}
    private void failure(Exception e){ui(()->{busy(false,null);message(e.getMessage()==null?"Something went wrong. Your scan files were kept.":e.getMessage(),true);});}
    private void copy(InputStream in,OutputStream out)throws IOException{byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}
    @Override public void onBackPressed(){if(!wide&&detailVisible){detailVisible=false;renderDetail();}else super.onBackPressed();}
}
