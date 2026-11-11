package com.webapp.herostudios;

import android.util.Base64;
import android.webkit.JavascriptInterface;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import org.json.JSONObject;
import java.io.*;
import java.util.Locale;

/** Android host: the C++ timeline engine is called in-process through JNI. */
public final class HeroNativeRuntime {
    private static final boolean LIBRARY_LOADED;
    private final Context context;
    private final File engineDir;
    static { boolean loaded; try { System.loadLibrary("hero_runtime"); loaded=true; } catch (UnsatisfiedLinkError | SecurityException error) { loaded=false; } LIBRARY_LOADED=loaded; }
    public HeroNativeRuntime(Context context) {
        this.context=context.getApplicationContext();
        this.engineDir=new File(this.context.getApplicationInfo().nativeLibraryDir);
    }
    private static native boolean nativeRenderAvailable();
    private static native String nativeCall(String method,String paramsJson,String cacheDir,String nativeLibraryDir);
    @JavascriptInterface public String runtimeInfo() {
        boolean ready=LIBRARY_LOADED && engineReady();
        return "{\"status\":\"loaded\",\"component\":\"hero-android-direct-jni-renderer\",\"mediaRendererAvailable\":"+ready+",\"sourceStagingAvailable\":"+ready+",\"mediaProbeAvailable\":"+LIBRARY_LOADED+",\"stage\":\"direct-jni-engine-call\",\"detail\":\""+jsonEscape(ready?"C++ timeline engine is linked into libhero_runtime and called directly through JNI":"JNI library or packaged FFmpeg tools are unavailable")+"\"}";
    }
    @JavascriptInterface public boolean renderAvailable() { return LIBRARY_LOADED && engineReady() && nativeRenderAvailable(); }
    @JavascriptInterface public synchronized String call(String method,String paramsJson) {
        if(!renderAvailable()) return errorJson("Direct JNI renderer is unavailable: required native library or FFmpeg tools are missing");
        try { return nativeCall(method,paramsJson,context.getCacheDir().getAbsolutePath(),engineDir.getAbsolutePath()); }
        catch(Exception error) { return errorJson(error.getMessage()==null?error.toString():error.getMessage()); }
    }
    @JavascriptInterface public synchronized String download(String outputPath) {
        File source=new File(outputPath);
        if(!source.isFile() || source.length()==0) return errorJson("Verified render output is missing or empty");
        String lower=source.getName().toLowerCase(Locale.ROOT);
        String mime=lower.endsWith(".wav")?"audio/wav":lower.endsWith(".flac")?"audio/flac":lower.endsWith(".mov")?"video/quicktime":lower.endsWith(".mkv")?"video/x-matroska":lower.endsWith(".webm")?"video/webm":lower.endsWith(".avi")?"video/x-msvideo":"video/mp4";
        try {
            if(Build.VERSION.SDK_INT>=29) {
                boolean audio=lower.endsWith(".wav")||lower.endsWith(".flac");
                ContentValues values=new ContentValues(); values.put(MediaStore.MediaColumns.DISPLAY_NAME,source.getName()); values.put(MediaStore.MediaColumns.MIME_TYPE,mime); values.put(MediaStore.MediaColumns.RELATIVE_PATH,(audio?Environment.DIRECTORY_MUSIC:Environment.DIRECTORY_MOVIES)+"/Hero Vid Studios");
                Uri collection=audio?MediaStore.Audio.Media.EXTERNAL_CONTENT_URI:MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                Uri uri=context.getContentResolver().insert(collection,values);
                if(uri==null) throw new IOException("Android could not create the output file");
                try(InputStream in=new FileInputStream(source); OutputStream out=context.getContentResolver().openOutputStream(uri)) { if(out==null) throw new IOException("Android could not open the output destination"); copy(in,out); }
                JSONObject result=new JSONObject(); result.put("status","ok"); result.put("outputUri",uri.toString()); result.put("bytes",source.length()); return result.toString();
            }
            File targetDir=context.getExternalFilesDir(Environment.DIRECTORY_MOVIES); if(targetDir==null) targetDir=new File(context.getFilesDir(),"Movies");
            if(!targetDir.exists()&&!targetDir.mkdirs()) throw new IOException("Unable to create output directory");
            File target=new File(targetDir,source.getName()); try(InputStream in=new FileInputStream(source);OutputStream out=new FileOutputStream(target)){copy(in,out);}
            JSONObject result=new JSONObject(); result.put("status","ok"); result.put("outputPath",target.getAbsolutePath()); result.put("bytes",target.length()); return result.toString();
        } catch(Exception error) { return errorJson(error.getMessage()); }
    }
    /** Called by the Activity when the WebView host is being destroyed.
     * The renderer owns no persistent Java-side process or descriptor; FFmpeg is
     * launched and joined by the in-process native engine for each render.
     */
    public synchronized void close() {
        // Intentionally no-op: there is no background process to terminate.
    }

    private boolean engineReady() {
        File ffmpeg = new File(engineDir, "libhero_ffmpeg.so");
        File ffprobe = new File(engineDir, "libhero_ffprobe.so");
        return ffmpeg.isFile() && ffmpeg.canExecute() && ffprobe.isFile() && ffprobe.canExecute();
    }
    private static void copy(InputStream in,OutputStream out)throws IOException { byte[] buffer=new byte[1024*1024]; int n; while((n=in.read(buffer))!=-1)out.write(buffer,0,n); out.flush(); }
    private static String jsonEscape(String value) { return value.replace("\\","\\\\").replace("\"","\\\"").replace("\n"," ").replace("\r"," "); }
    private static String errorJson(String message) { return "{\"status\":\"error\",\"message\":\""+jsonEscape(message==null?"Unknown native engine error":message)+"\"}"; }
}
