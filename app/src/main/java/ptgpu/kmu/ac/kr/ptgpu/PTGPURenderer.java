package ptgpu.kmu.ac.kr.ptgpu;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.opengl.GLSurfaceView;
import android.opengl.GLSurfaceView.Renderer;
import android.opengl.GLU;
import android.opengl.GLUtils;
import android.os.Environment;
import android.util.Log;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.opengl.GLES20;

/**
 * Created by user on 2017-12-23.
 */

public class PTGPURenderer implements GLSurfaceView.Renderer {
    int texW = 640;//canvas.getWidth();
    int texH = 480;//canvas.getHeight();
    int[] textures = new int[1];

    private int inv = 0;
    private final boolean bFile = false;

    /** The buffer holding the vertices */
    private FloatBuffer vertexBuffer;
    /** The buffer holding the texture coordinates */
    private FloatBuffer textureBuffer;

    Context context;

    /** The initial vertex definition */
    private float vertices[] = {
            -1.0f, -1.0f, 0.0f,     //Bottom Left
            1.0f, -1.0f, 0.0f,      //Bottom Right
            -1.0f, 1.0f, 0.0f,      //Top Left
            1.0f, 1.0f, 0.0f        //Top Right
    };

    /** The initial texture coordinates (u, v) */
    private float texture[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
    };

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    public native void initSmallPtGPU(int u, int f, String k, int w, int h, String s, String r, AssetManager asset);
    public native int[] updateRendering();
    public native void finishRendering();

    // Used to load the 'native-lib' library on application startup.
    static
    {
        System.loadLibrary("native-lib");
    }

    public PTGPURenderer(Context context)
    {
        this.context = context;
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config)
    {
        Log.e("PTGPURenderer", "Start of onSurfaceCreated");

        //
        ByteBuffer byteBuf = ByteBuffer.allocateDirect(vertices.length * 4);
        byteBuf.order(ByteOrder.nativeOrder());
        vertexBuffer = byteBuf.asFloatBuffer();
        vertexBuffer.put(vertices);
        vertexBuffer.position(0);

        //
        byteBuf = ByteBuffer.allocateDirect(texture.length * 4);
        byteBuf.order(ByteOrder.nativeOrder());
        textureBuffer = byteBuf.asFloatBuffer();
        textureBuffer.put(texture);
        textureBuffer.position(0);

        initSmallPtGPU(1, 128, "kernels/preprocessed_rendering_kernel.cl", texW, texH, "scenes/ant.ply", Environment.getExternalStorageDirectory() + "/" + context.getPackageName(), context.getAssets());

        try {
            gl.glEnable(GL10.GL_TEXTURE_2D);                    //Enable Texture Mapping
            gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);            //Black Background

            gl.glGenTextures(1, textures, 0);
        } catch (Exception e) {
            // TODO Auto-generated catch block
            Log.d("Excption at the onSurfaceCreated",e.getMessage());
        }

        Log.d("PTGPURenderer", "End of onSurfaceCreated");
    }

    public void onDrawFrame(GL10 gl)
    {
        Log.i("PTGPURenderer", "Start of onDrawFrame");

        int[] arr_pixels = updateRendering();

        if (arr_pixels == null)
        {
            Log.e("PTGPURenderer", "null is returned!!!");
            return;
        }

        Bitmap bitmap = Bitmap.createBitmap(arr_pixels, texW, texH, Bitmap.Config.ARGB_8888);

        if (bFile) {
            String strFN = Environment.getExternalStorageDirectory()+"/Download/images/image_" + new Integer(inv).toString() + ".png";
            FileOutputStream out = null;

            try {
                out = new FileOutputStream(strFN);
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out); // bmp is your Bitmap instance
                // PNG is a lossless format, the compression factor (100) is ignored
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                try {
                    if (out != null) {
                        out.close();
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        try {
            long start = System.currentTimeMillis();

            gl.glClear(GL10.GL_COLOR_BUFFER_BIT);
            gl.glBindTexture(GL10.GL_TEXTURE_2D, textures[0]);

            // Create Linear Filtered Texture and bind it to texture
            GLUtils.texImage2D(GL10.GL_TEXTURE_2D, 0, bitmap, 0);

            gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_NEAREST);
            gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_NEAREST);

            // Clean up
            bitmap.recycle();

            //Enable the vertex, texture and normal state
            gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL10.GL_TEXTURE_COORD_ARRAY);

            //Point to our buffers
            gl.glVertexPointer(3, GL10.GL_FLOAT, 0, vertexBuffer);
            gl.glTexCoordPointer(2, GL10.GL_FLOAT, 0, textureBuffer);

            //Draw the vertices as triangle strip
            gl.glDrawArrays(GL10.GL_TRIANGLE_STRIP, 0, vertices.length / 3);

            //Disable the client state before leaving
            gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL10.GL_TEXTURE_COORD_ARRAY);

            gl.glBindTexture(GL10.GL_TEXTURE_2D, 0);

            long end = System.currentTimeMillis();
            int diff = (int) (end-start);

            Log.e("PTGPURenderer", "Drawing time: "+new Integer(diff).toString()+" msec");
        } catch (Exception e) {
            // TODO Auto-generated catch block
            Log.e("Excption at the onDrawFrame",e.getMessage());
        }

        Log.i("PTGPURenderer", "End of onDrawFrame, "+String.valueOf(inv));
        inv++;
    }

    public void onSurfaceChanged(GL10 gl, int w, int h)
    {
        Log.e("PTGPURenderer", "Start of onSurfaceChanged");
        try {
            if(h == 0) {                       //Prevent A Divide By Zero By
                h = 1;                         //Making Height Equal One
            }

            gl.glViewport(0, 0, w, h);//specifies transformation from normalized device coordinates to window coordinates

            gl.glMatrixMode(GL10.GL_PROJECTION); //Select The Projection Matrix
            gl.glLoadIdentity();//Reset The Projection Matrix

            float ratio = (float) w / h;
            GLU.gluPerspective(gl, 45.0f, ratio, 0.1f, 100.0f);

            gl.glMatrixMode(GL10.GL_MODELVIEW);//Select The Modelview Matrix
            gl.glLoadIdentity();//Reset The Modelview Matrix
            gl.glTranslatef(0.0f, 0.0f, -3.0f);
        } catch (Exception e) {
            // TODO Auto-generated catch block
            Log.d("Excption at the onSurfaceChanged",e.getMessage());
        }
        Log.e("PTGPURenderer", "End of onSurfaceChanged");
    }
}
