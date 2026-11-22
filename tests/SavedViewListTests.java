package com.splativerse.quest;
import org.json.*;
import java.util.UUID;
public final class SavedViewListTests {
    static void check(boolean ok){if(!ok)throw new AssertionError();}
    static JSONObject catalog(int count)throws Exception {
        JSONArray views=new JSONArray();for(int i=0;i<count;i++)views.put(new JSONObject().put("id",UUID.randomUUID().toString()).put("name",i==2?"京都 — a long named View":"View "+i).put("pose",new JSONObject().put("position",new JSONArray(new double[]{i,2,3})).put("forward",new JSONArray(new double[]{0,0,-1}))));
        return new JSONObject().put("views",views);
    }
    public static void main(String[] args)throws Exception {
        SavedViewList empty=new SavedViewList(catalog(0));check(empty.pages()==1&&empty.names(0).length==0&&empty.pose(0,0)==null);
        for(int count:new int[]{1,3,4,99,100}){
            JSONObject original=catalog(count);String before=original.toString();SavedViewList list=new SavedViewList(original);
            int seen=0;for(int page=0;page<list.pages();page++)for(int row=0;row<list.names(page).length;row++){
                float[] pose=list.pose(page,row);check(pose[0]==seen++);pose[0]=-999;check(list.pose(page,row)[0]==seen-1);
            }
            check(seen==count&&before.equals(original.toString()));check(list.page(-1)==0&&list.page(500)==list.pages()-1);
            check(list.pose(0,-1)==null&&list.pose(0,3)==null);
            if(count%3!=0)check(list.pose(list.pages()-1,count%3)==null);
            // Snapshot cannot be silently retargeted by a catalog edit while visible.
            original.getJSONArray("views").getJSONObject(0).getJSONObject("pose").getJSONArray("position").put(0,999);
            check(list.pose(0,0)[0]==0);
        }
        JSONObject malformed=catalog(1);malformed.getJSONArray("views").getJSONObject(0).getJSONObject("pose").put("forward",new JSONArray(new double[]{0,0,0}));
        boolean rejected=false;try{new SavedViewList(malformed);}catch(Exception e){rejected=true;}check(rejected);
        System.out.println("PASS: saved View pagination, empty and partial pages, 100-View limit, immutable pose mapping and invalid pose rejection");
    }
}
