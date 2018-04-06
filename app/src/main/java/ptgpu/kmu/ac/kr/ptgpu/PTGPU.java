package ptgpu.kmu.ac.kr.ptgpu;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

public class PTGPU extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Example of a call to a native method
        pgv = new PTGPUGLSurfaceView(this);
        setContentView(pgv);
     }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    PTGPUGLSurfaceView  pgv;
 }
