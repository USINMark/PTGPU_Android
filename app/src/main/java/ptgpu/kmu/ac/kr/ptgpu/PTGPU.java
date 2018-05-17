package ptgpu.kmu.ac.kr.ptgpu;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class PTGPU extends Activity {
    private static final int REQUEST_READ_WRITE_EXTERNAL_STORAGE = 1;

    public String copyDirorfileFromAssetManager(String arg_assetDir, String arg_destinationDir) throws IOException
    {
        File sd_path = Environment.getExternalStorageDirectory();
        String dest_dir_path = sd_path + addLeadingSlash(arg_destinationDir);
        File dest_dir = new File(dest_dir_path);

        createDir(dest_dir);

        AssetManager asset_manager = getAssets();
        String[] files = asset_manager.list(arg_assetDir);

        for (int i = 0; i < files.length; i++)
        {
            String abs_asset_file_path = addTrailingSlash(arg_assetDir) + files[i];
            String sub_files[] = asset_manager.list(abs_asset_file_path);

            if (sub_files.length == 0)
            {
                // It is a file
                String dest_file_path = addTrailingSlash(dest_dir_path) + files[i];
                copyAssetFile(abs_asset_file_path, dest_file_path);
            }
            else
            {
                // It is a sub directory
                copyDirorfileFromAssetManager(abs_asset_file_path, addTrailingSlash(arg_destinationDir) + files[i]);
            }
        }

        return dest_dir_path;
    }

    public void copyAssetFile(String assetFilePath, String destinationFilePath) throws IOException
    {
        InputStream in = getApplicationContext().getAssets().open(assetFilePath);
        OutputStream out = new FileOutputStream(destinationFilePath);

        byte[] buf = new byte[1024];
        int len;

        while ((len = in.read(buf)) > 0)
            out.write(buf, 0, len);

        in.close();
        out.close();
    }

    public String addTrailingSlash(String path)
    {
        if (path.charAt(path.length() - 1) != '/')
            path += "/";

        return path;
    }

    public String addLeadingSlash(String path)
    {
        if (path.charAt(0) != '/')
            path = "/" + path;

        return path;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        switch (requestCode) {
            case REQUEST_READ_WRITE_EXTERNAL_STORAGE:
                {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // permission was granted, yay! Do the
                    // contacts-related task you need to do.
                    pgv = new PTGPUGLSurfaceView(this);
                    setContentView(pgv);
                } else {
                    // permission denied, boo! Disable the
                    // functionality that depends on this permission.
                    System.exit(0);
                }

                return;
            }
        }
    }

    public void createDir(File dir) throws IOException
    {
        if (dir.exists())
        {
            if (!dir.isDirectory())
                throw new IOException("Can't create directory, a file is in the way");
        } else {
            dir.mkdirs();
            if (!dir.isDirectory())
                throw new IOException("Unable to create directory");
        }
    }

    void deleteRecursive(File file) {
        if (file.isDirectory())
            for (File child : file.listFiles())
                deleteRecursive(child);

        file.delete();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        String strDst = getPackageName();

        File fileOrDirectory = new File(strDst);
        deleteRecursive(fileOrDirectory);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        try
        {
            String strPkg = getPackageName();
            File FolderInCache = new File(strPkg);

            if (!FolderInCache.exists())
                copyDirorfileFromAssetManager("sdcard", strPkg);
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }

        // Here, thisActivity is the current activity
        if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED ||
                checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {

            // No explanation needed, we can request the permission.
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST_READ_WRITE_EXTERNAL_STORAGE);
        }
        else {
            pgv = new PTGPUGLSurfaceView(this);
            setContentView(pgv);
        }
     }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    PTGPUGLSurfaceView  pgv;
 }
