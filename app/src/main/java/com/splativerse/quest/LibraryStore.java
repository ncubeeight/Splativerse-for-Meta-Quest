package com.splativerse.quest;

import org.json.*;
import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.GZIPInputStream;

/** File-backed metadata. Scan bytes are never rewritten by library/view operations. */
public final class LibraryStore {
    private static final Object LOCK=new Object();
    // Shared queue lets the returning library observe saves queued by the viewer.
    public static final java.util.concurrent.ExecutorService IO=java.util.concurrent.Executors.newSingleThreadExecutor();
    public static final long MAX_FILE=128L*1024*1024;
    public static final int MAX_SPLATS=1000000;
    private final File root;
    public LibraryStore(File root)throws IOException{this.root=root;if(!root.isDirectory()&&!root.mkdirs())throw new IOException("Could not create the library metadata folder.");}
    public static final class Item {
        public final File file;public final String digest,format;public final long bytes,modified,count;public final String issue;
        public final List<String> filenames=new ArrayList<>();
        public String title;public JSONObject settings,views;public int radius;public long lastOpened;public int copies=1;
        Item(File f,String hash,String format,long count,String issue){file=f;digest=hash;this.format=format;bytes=f.length();modified=f.lastModified();this.count=count;this.issue=issue;title=f.getName();filenames.add(f.getName());}
        public boolean canOpen(){return issue.isEmpty()&&count>0&&count<=MAX_SPLATS&&bytes<=MAX_FILE;}
        public int viewCount(){return views==null?0:views.optJSONArray("views").length();}
    }
    public static Item mergeCopies(Item first,Item next){
        if(!first.digest.equals(next.digest))throw new IllegalArgumentException("Different scans cannot be grouped.");
        boolean preferNext=first.file.getName().matches("[0-9a-f]{64}\\.[^.]+")&&!next.file.getName().matches("[0-9a-f]{64}\\.[^.]+");
        Item chosen=preferNext?next:first,other=preferNext?first:next;
        chosen.copies+=other.copies;chosen.filenames.addAll(other.filenames);return chosen;
    }
    public static boolean supported(String name){String n=name.toLowerCase(Locale.ROOT);return n.endsWith(".ply")||n.endsWith(".spz")||n.endsWith(".splat");}
    public static String fingerprint(File file)throws Exception{
        long size=file.length(),stamp=file.lastModified();MessageDigest hash=MessageDigest.getInstance("SHA-256");long total=0;
        try(InputStream in=new FileInputStream(file)){byte[] block=new byte[1024*1024];int n;while((n=in.read(block))!=-1){if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Library refresh cancelled.");hash.update(block,0,n);total+=n;}}
        if(total!=size||file.length()!=size||file.lastModified()!=stamp)throw new IOException("The scan changed while it was being read. Refresh the library.");
        StringBuilder result=new StringBuilder();for(byte b:hash.digest())result.append(String.format(Locale.ROOT,"%02x",b&255));return result.toString();
    }
    public static void verify(File file,String digest)throws Exception{if(!file.isFile()||!fingerprint(file).equals(digest))throw new IOException("This scan changed or was removed. Refresh the library before using its Views.");}
    private static String line(InputStream in,int[] remaining)throws IOException{
        ByteArrayOutputStream out=new ByteArrayOutputStream();int b;
        while((b=in.read())!=-1){if(--remaining[0]<0)throw new IOException("PLY header exceeds 64 KiB.");if(b==10)break;if(b!=13)out.write(b);}
        if(b==-1&&out.size()==0)throw new EOFException("Incomplete PLY header.");return new String(out.toByteArray(),StandardCharsets.US_ASCII);
    }
    public static Item inspect(File file)throws Exception{
        String hash=fingerprint(file),format=file.getName().substring(file.getName().lastIndexOf('.')+1).toUpperCase(Locale.ROOT),issue="";long count=-1;
        try{
            if(file.length()==0)throw new IOException("This scan is empty.");
            if(format.equals("SPZ")){
                try(DataInputStream in=new DataInputStream(new GZIPInputStream(new FileInputStream(file)))){
                    byte[] h=new byte[16];in.readFully(h);ByteBuffer b=ByteBuffer.wrap(h).order(ByteOrder.LITTLE_ENDIAN);
                    if(b.getInt()!=0x5053474e)throw new IOException("Invalid SPZ header.");int version=b.getInt();count=Integer.toUnsignedLong(b.getInt());
                    if(version<1||version>3)throw new IOException("SPZ version "+version+" is not supported yet.");
                    if((h[12]&255)>3||(h[13]&255)>24||((h[14]&255)&~1)!=0)throw new IOException("Unsupported SPZ metadata.");
                }
            }else if(format.equals("SPLAT")){if(file.length()%32!=0)throw new IOException("Incomplete SPLAT record.");count=file.length()/32;}
            else if(format.equals("PLY")){
                try(InputStream in=new BufferedInputStream(new FileInputStream(file))){int[] left={65536};if(!line(in,left).equals("ply"))throw new IOException("Invalid PLY header.");String value;boolean formatOK=false;
                    while(!(value=line(in,left)).equals("end_header")){String[] tokens=value.trim().split("\\s+");if(value.startsWith("format "))formatOK=value.startsWith("format ascii ")||value.startsWith("format binary_little_endian ");if(tokens.length==3&&tokens[0].equals("element")&&tokens[1].equals("vertex"))count=Long.parseLong(tokens[2]);}
                    if(!formatOK)throw new IOException("Use ASCII or little-endian PLY.");
                }
            }else throw new IOException("Unsupported scan format.");
            if(count<=0)throw new IOException("No splats found in the scan header.");
            if(count>MAX_SPLATS)issue="Above the current 1,000,000-splat viewer limit.";
            if(file.length()>MAX_FILE)issue="Above the current 128 MiB file limit.";
        }catch(Exception e){issue=e.getMessage()==null?"Could not read the scan header.":e.getMessage();}
        return new Item(file,hash,format,count,issue);
    }
    public static JSONObject defaultSettings()throws JSONException{return new JSONObject().put("metresPerUnit",1).put("upAxis","yDown").put("floorOffset",0).put("bubbleRadius",60).put("spawnMode","denseRegion").put("splatFilter","source");}
    public static JSONObject emptyViews()throws JSONException{return new JSONObject().put("views",new JSONArray());}
    private static JSONObject cloneObject(JSONObject value)throws JSONException{return new JSONObject(value.toString());}
    private static void digestValid(String digest)throws IOException{if(digest==null||!digest.matches("[0-9a-f]{64}"))throw new IOException("Invalid scan fingerprint.");}
    public static void validatePose(JSONObject pose)throws Exception{
        double length=0;
        for(String key:new String[]{"position","forward"}){JSONArray a=pose.getJSONArray(key);if(a.length()!=3)throw new IOException("A View needs three position and heading coordinates.");for(int i=0;i<3;i++){Object raw=a.get(i);if(!(raw instanceof Number))throw new IOException("Invalid View coordinate.");double v=((Number)raw).doubleValue();if(!Double.isFinite(v)||Math.abs(v)>=1e12)throw new IOException("Invalid View coordinate.");if(key.equals("forward"))length+=v*v;}}
        if(length<=1e-10)throw new IOException("A View needs a nonzero heading.");
    }
    private static String name(String value)throws IOException{
        String trimmed=value==null?"":value.trim();if(trimmed.isEmpty()||trimmed.codePointCount(0,trimmed.length())>80)throw new IOException("Choose a name of 1–80 characters.");
        for(int i=0;i<trimmed.length();i++)if(Character.isISOControl(trimmed.charAt(i)))throw new IOException("Names cannot contain control characters.");return trimmed;
    }
    private static String uuid(String text)throws IOException{try{UUID id=UUID.fromString(text);if(!id.toString().equalsIgnoreCase(text))throw new IllegalArgumentException();return id.toString();}catch(Exception e){throw new IOException("Invalid saved View identifier.");}}
    public static void validateViews(JSONObject value)throws Exception{
        JSONArray list=value.getJSONArray("views");if(list.length()>100)throw new IOException("A scan can have up to 100 saved Views.");Set<String> ids=new HashSet<>();
        for(int i=0;i<list.length();i++){JSONObject view=list.getJSONObject(i);if(!(view.get("name") instanceof String))throw new IOException("Invalid View name.");if(!ids.add(uuid(view.getString("id"))))throw new IOException("Duplicate View identifier.");name(view.getString("name"));validatePose(view.getJSONObject("pose"));}
        if(value.has("defaultID")&&!value.isNull("defaultID")&&!ids.contains(uuid(value.getString("defaultID"))))throw new IOException("The starting View is missing.");
    }
    public static void validateSettings(JSONObject s)throws Exception{
        for(String key:new String[]{"metresPerUnit","floorOffset","bubbleRadius"})if(!(s.get(key) instanceof Number))throw new IOException("Invalid portable scan settings.");
        double units=s.getDouble("metresPerUnit"),floor=s.getDouble("floorOffset"),radius=s.getDouble("bubbleRadius");
        if(!Float.isFinite((float)units)||(float)units<=0||!Float.isFinite((float)floor)||!Double.isFinite(radius)||radius<60||radius>360)throw new IOException("Invalid portable scan settings.");
        if(!Arrays.asList("yDown","yUp","zUp").contains(s.getString("upAxis"))||!Arrays.asList("denseRegion","scanCenter").contains(s.getString("spawnMode"))||!Arrays.asList("source","pixel").contains(s.getString("splatFilter")))throw new IOException("Unsupported scan settings.");
    }
    public static int checkedRadius(int radius)throws IOException{for(int r:new int[]{0,30,60,120,180,240,300,360})if(radius==r)return r;throw new IOException("Unsupported live radius.");}
    private JSONObject read(String filename)throws Exception{
        File file=new File(root,filename);if(!file.exists())return new JSONObject();if(!file.isFile()||file.length()>16*1024*1024)throw new IOException("The library metadata cannot be read. Your scans have not been changed.");
        byte[] data=Files.readAllBytes(file.toPath());try{return new JSONObject(new String(data,StandardCharsets.UTF_8));}catch(JSONException e){throw new IOException("The library metadata is damaged. It has been kept for recovery; no Views were overwritten.",e);}
    }
    private void write(String filename,JSONObject value)throws Exception{
        byte[] bytes=value.toString(2).getBytes(StandardCharsets.UTF_8);if(bytes.length>16*1024*1024)throw new IOException("Library metadata is too large.");
        File tmp=File.createTempFile("metadata-",".tmp",root);
        try{try(FileOutputStream out=new FileOutputStream(tmp)){out.write(bytes);out.getFD().sync();}Files.move(tmp.toPath(),new File(root,filename).toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);}finally{if(tmp.exists())tmp.delete();}
    }
    private JSONObject catalog()throws Exception{JSONObject c=read("library.json");if(c.length()==0)c.put("version",1).put("assets",new JSONObject());if(c.getInt("version")!=1)throw new IOException("This library needs a newer app version.");c.getJSONObject("assets");return c;}
    private JSONObject viewsCatalog()throws Exception{JSONObject all=read("viewpoints.json");Iterator<String> keys=all.keys();while(keys.hasNext()){String key=keys.next();digestValid(key);validateViews(all.getJSONObject(key));}return all;}
    public JSONObject views(String digest)throws Exception{synchronized(LOCK){digestValid(digest);JSONObject all=viewsCatalog();return all.has(digest)?cloneObject(all.getJSONObject(digest)):emptyViews();}}
    public JSONObject metadata(String digest)throws Exception{synchronized(LOCK){digestValid(digest);JSONObject a=catalog().getJSONObject("assets").optJSONObject(digest);return a==null?null:cloneObject(a);}}
    public void configure(String digest,JSONObject settings,int radius,String title,boolean opened)throws Exception{synchronized(LOCK){
        digestValid(digest);validateSettings(settings);checkedRadius(radius);JSONObject c=catalog(),assets=c.getJSONObject("assets"),a=assets.optJSONObject(digest);if(a==null)a=new JSONObject();
        a.put("settings",cloneObject(settings)).put("liveRadius",radius);if(title!=null)a.put("title",name(title));if(opened)a.put("lastOpened",System.currentTimeMillis());assets.put(digest,a);write("library.json",c);
    }}
    public void decorate(Item item,JSONObject migratedSettings,int migratedRadius,String migratedTitle)throws Exception{synchronized(LOCK){
        JSONObject a=metadata(item.digest);if(a==null){configure(item.digest,migratedSettings,migratedRadius,migratedTitle,false);a=metadata(item.digest);}
        item.settings=a.getJSONObject("settings");validateSettings(item.settings);item.radius=checkedRadius(a.optInt("liveRadius",60));item.title=a.optString("title",item.file.getName());item.lastOpened=a.optLong("lastOpened",0);item.views=views(item.digest);
    }}
    public static JSONObject startingPose(JSONObject views)throws Exception{
        String id=views.optString("defaultID","");JSONArray list=views.getJSONArray("views");for(int i=0;i<list.length();i++){JSONObject v=list.getJSONObject(i);if(v.getString("id").equalsIgnoreCase(id))return cloneObject(v.getJSONObject("pose"));}return null;
    }
    public JSONObject saveView(File file,String digest,JSONObject pose,String requestedName,String updateID)throws Exception{
        verify(file,digest);validatePose(pose);
        synchronized(LOCK){JSONObject all=viewsCatalog(),set=all.has(digest)?all.getJSONObject(digest):emptyViews();JSONArray list=set.getJSONArray("views");JSONObject saved=null;
            if(updateID!=null){for(int i=0;i<list.length();i++)if(list.getJSONObject(i).getString("id").equalsIgnoreCase(updateID)){saved=list.getJSONObject(i);saved.put("pose",cloneObject(pose));break;}if(saved==null)throw new IOException("The View to update no longer exists.");}
            else{if(list.length()>=100)throw new IOException("This scan already has 100 Views. Remove a View before saving another.");String chosen=requestedName;
                if(chosen==null){Set<String> names=new HashSet<>();for(int i=0;i<list.length();i++)names.add(list.getJSONObject(i).getString("name").toLowerCase(Locale.ROOT));int n=1;while(names.contains("view "+n))n++;chosen="View "+n;}
                saved=new JSONObject().put("id",UUID.randomUUID().toString()).put("name",name(chosen)).put("pose",cloneObject(pose));list.put(saved);}
            validateViews(set);all.put(digest,set);write("viewpoints.json",all);return cloneObject(saved);
        }
    }
    public void editView(String digest,String id,String operation,String newName)throws Exception{synchronized(LOCK){
        digestValid(digest);JSONObject all=viewsCatalog(),set=all.has(digest)?all.getJSONObject(digest):emptyViews();JSONArray list=set.getJSONArray("views");int found=-1;
        for(int i=0;i<list.length();i++)if(list.getJSONObject(i).getString("id").equalsIgnoreCase(id)){found=i;break;}
        if(operation.equals("automatic"))set.remove("defaultID");
        else{if(found<0)throw new IOException("This View no longer exists.");JSONObject v=list.getJSONObject(found);
            if(operation.equals("rename"))v.put("name",name(newName));
            else if(operation.equals("default"))set.put("defaultID",v.getString("id"));
            else if(operation.equals("delete")){JSONArray next=new JSONArray();for(int i=0;i<list.length();i++)if(i!=found)next.put(list.getJSONObject(i));set.put("views",next);if(v.getString("id").equalsIgnoreCase(set.optString("defaultID","")))set.remove("defaultID");}
            else throw new IOException("Unknown View action.");}
        validateViews(set);all.put(digest,set);write("viewpoints.json",all);
    }}
    public static int naturalCompare(String a,String b){
        int i=0,j=0;while(i<a.length()&&j<b.length()){
            if(Character.isDigit(a.charAt(i))&&Character.isDigit(b.charAt(j))){int ae=i,be=j;while(ae<a.length()&&Character.isDigit(a.charAt(ae)))ae++;while(be<b.length()&&Character.isDigit(b.charAt(be)))be++;String an=a.substring(i,ae).replaceFirst("^0+(?!$)",""),bn=b.substring(j,be).replaceFirst("^0+(?!$)","");int c=Integer.compare(an.length(),bn.length());if(c==0)c=an.compareTo(bn);if(c!=0)return c;i=ae;j=be;}
            else{int c=Character.compare(Character.toLowerCase(a.charAt(i)),Character.toLowerCase(b.charAt(j)));if(c!=0)return c;i++;j++;}}
        int c=Integer.compare(a.length()-i,b.length()-j);return c!=0?c:a.compareTo(b);
    }
}
