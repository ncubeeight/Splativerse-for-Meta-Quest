package com.splativerse.quest;
import org.json.*;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

public final class LibraryStoreTests {
    interface Task{void run()throws Exception;}
    static void check(boolean v){if(!v)throw new AssertionError();}
    static void rejects(Task t)throws Exception{try{t.run();}catch(Exception expected){return;}throw new AssertionError("Expected rejection");}
    static JSONObject pose()throws Exception{return new JSONObject("{\"position\":[12.5,-1.75,3],\"forward\":[0,0,1]}");}
    static String read(File f)throws Exception{return new String(Files.readAllBytes(f.toPath()),StandardCharsets.UTF_8);}
    public static void main(String[] args)throws Exception{
        File work=new File(args[0]);work.mkdirs();File scan=new File(work,"scan.splat");byte[] original=new byte[64];new Random(123).nextBytes(original);Files.write(scan.toPath(),original);long stamp=scan.lastModified();
        String digest=LibraryStore.fingerprint(scan);File root=new File(work,"library");LibraryStore store=new LibraryStore(root);
        JSONObject settings=LibraryStore.defaultSettings();store.configure(digest,settings,30,"Room 2",true);check(store.metadata(digest).getInt("liveRadius")==30);check(store.metadata(digest).getJSONObject("settings").getInt("bubbleRadius")==60);
        JSONObject a=store.saveView(scan,digest,pose(),"Entrance",null);String id=a.getString("id");store.editView(digest,id,"default",null);check(LibraryStore.startingPose(store.views(digest)).getJSONArray("position").getDouble(0)==12.5);
        store.editView(digest,id,"rename","Entry 🚪");JSONObject changed=pose();changed.getJSONArray("position").put(0,20);store.saveView(scan,digest,changed,null,id);check(store.views(digest).getJSONArray("views").length()==1);check(LibraryStore.startingPose(store.views(digest)).getJSONArray("position").getDouble(0)==20);
        File renamed=new File(work,"renamed.splat");Files.move(scan.toPath(),renamed.toPath());check(LibraryStore.fingerprint(renamed).equals(digest));check(new LibraryStore(root).views(digest).getJSONArray("views").getJSONObject(0).getString("name").equals("Entry 🚪"));
        File hashed=new File(work,digest+".splat");Files.copy(renamed.toPath(),hashed.toPath());LibraryStore.Item grouped=LibraryStore.mergeCopies(LibraryStore.inspect(hashed),LibraryStore.inspect(renamed));check(grouped.file.equals(renamed)&&grouped.copies==2&&grouped.filenames.contains(hashed.getName()));
        File copy=new File(work,"reimported.splat");Files.copy(renamed.toPath(),copy.toPath());check(LibraryStore.fingerprint(copy).equals(digest));check(Arrays.equals(Files.readAllBytes(renamed.toPath()),original));check(renamed.lastModified()==stamp);
        byte[] altered=original.clone();altered[0]^=1;Files.write(copy.toPath(),altered);rejects(()->store.saveView(copy,digest,pose(),null,null));check(!LibraryStore.fingerprint(copy).equals(digest));
        store.editView(digest,id,"delete",null);check(LibraryStore.startingPose(store.views(digest))==null);check(store.views(digest).getJSONArray("views").length()==0);
        // Two store instances must never overwrite each other's new View.
        ExecutorService pool=Executors.newFixedThreadPool(2);List<Future<?>> futures=new ArrayList<>();for(int n=0;n<20;n++)futures.add(pool.submit(()->{try{new LibraryStore(root).saveView(renamed,digest,pose(),null,null);}catch(Exception e){throw new RuntimeException(e);}}));for(Future<?> f:futures)f.get();pool.shutdown();
        check(store.views(digest).getJSONArray("views").length()==20);
        for(int n=20;n<100;n++)store.saveView(renamed,digest,pose(),null,null);rejects(()->store.saveView(renamed,digest,pose(),null,null));check(store.views(digest).getJSONArray("views").length()==100);
        JSONObject set=store.views(digest);set.put("defaultID",UUID.randomUUID().toString());rejects(()->LibraryStore.validateViews(set));set.remove("defaultID");JSONArray list=set.getJSONArray("views");list.put(1,list.getJSONObject(0));rejects(()->LibraryStore.validateViews(set));
        rejects(()->LibraryStore.validatePose(new JSONObject("{\"position\":[0,1],\"forward\":[0,0,1]}")));
        rejects(()->LibraryStore.validatePose(new JSONObject("{\"position\":[0,1,2],\"forward\":[0,0,0]}")));
        rejects(()->LibraryStore.validatePose(new JSONObject("{\"position\":[\"0\",1,2],\"forward\":[0,0,1]}")));
        rejects(()->LibraryStore.validatePose(new JSONObject("{\"position\":[1e12,1,2],\"forward\":[0,0,1]}")));
        rejects(()->store.editView(digest,store.views(digest).getJSONArray("views").getJSONObject(0).getString("id"),"rename","   "));
        check(LibraryStore.naturalCompare("Room 2","Room 10")<0);check(LibraryStore.inspect(renamed).count==2);check(LibraryStore.inspect(copy).issue.isEmpty());
        File bad=new File(work,"bad.spz");Files.write(bad.toPath(),new byte[]{1,2,3});check(!LibraryStore.inspect(bad).canOpen());
        JSONObject badSettings=LibraryStore.defaultSettings().put("bubbleRadius","60");rejects(()->LibraryStore.validateSettings(badSettings));
        JSONObject badName=LibraryStore.emptyViews().put("views",new JSONArray().put(new JSONObject().put("id",UUID.randomUUID().toString()).put("name",123).put("pose",pose())));rejects(()->LibraryStore.validateViews(badName));
        // A damaged catalog is preserved, and failed edits cannot destroy its bytes.
        File views=new File(root,"viewpoints.json");String before=read(views);Files.write(views.toPath(),"{broken".getBytes(StandardCharsets.UTF_8));rejects(()->store.saveView(renamed,digest,pose(),null,null));check(read(views).equals("{broken"));Files.write(views.toPath(),before.getBytes(StandardCharsets.UTF_8));
        File library=new File(root,"library.json");String old=read(library);Files.delete(library.toPath());Files.createDirectory(library.toPath());rejects(()->store.configure(digest,settings,60,null,false));Files.delete(library.toPath());Files.write(library.toPath(),old.getBytes(StandardCharsets.UTF_8));check(store.views(digest).getJSONArray("views").length()==100);
        // Read a fixture encoded by the actual Vision Pro model, then write it for Swift to decode.
        if(args.length>1){JSONObject vision=new JSONObject(read(new File(args[1])));Iterator<String> keys=vision.keys();while(keys.hasNext()){String key=keys.next();LibraryStore.validateViews(vision.getJSONObject(key));}File compat=new File(work,"compat");compat.mkdirs();Files.write(new File(compat,"viewpoints.json").toPath(),vision.toString(2).getBytes(StandardCharsets.UTF_8));LibraryStore bridge=new LibraryStore(compat);JSONObject imported=bridge.views(digest);check(imported.getJSONArray("views").length()==1);bridge.saveView(renamed,digest,pose(),"Quest entrance",null);Files.write(new File(work,"quest-settings.json").toPath(),settings.toString().getBytes(StandardCharsets.UTF_8));}
        System.out.println("PASS: library identity/reimport; source preservation; View CRUD/default; concurrent saves; 100-View limit; validation; damaged metadata preservation; header inspection"+(args.length>1?"; Vision JSON import/export":"")+"");
    }
}
