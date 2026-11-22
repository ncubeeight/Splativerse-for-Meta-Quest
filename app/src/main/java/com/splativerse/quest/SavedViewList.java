package com.splativerse.quest;

import org.json.*;

/** Immutable portable View snapshot with bounded, three-row pages. */
public final class SavedViewList {
    public static final int PAGE_SIZE=3;
    private final String[] names;
    private final float[][] poses;
    public SavedViewList(JSONObject catalog)throws Exception {
        LibraryStore.validateViews(catalog);
        JSONArray views=catalog.getJSONArray("views");
        names=new String[views.length()];poses=new float[views.length()][6];
        for(int i=0;i<names.length;i++){
            JSONObject v=views.getJSONObject(i),p=v.getJSONObject("pose");names[i]=v.getString("name");
            for(int k=0;k<3;k++){poses[i][k]=(float)p.getJSONArray("position").getDouble(k);poses[i][k+3]=(float)p.getJSONArray("forward").getDouble(k);}
        }
    }
    public int count(){return names.length;}
    public int pages(){return Math.max(1,(count()+PAGE_SIZE-1)/PAGE_SIZE);}
    public int page(int value){return Math.max(0,Math.min(value,pages()-1));}
    public String[] names(int page){
        int first=page(page)*PAGE_SIZE,n=Math.min(PAGE_SIZE,count()-first);String[] result=new String[n];
        System.arraycopy(names,first,result,0,n);return result;
    }
    public float[] pose(int page,int row){
        if(row<0||row>=PAGE_SIZE)return null;int index=page(page)*PAGE_SIZE+row;
        return index<count()?poses[index].clone():null;
    }
}
