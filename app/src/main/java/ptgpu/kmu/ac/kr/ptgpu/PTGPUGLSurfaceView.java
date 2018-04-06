package ptgpu.kmu.ac.kr.ptgpu;

import android.content.Context;
import android.graphics.Canvas;
import android.opengl.GLSurfaceView;

/**
 * Created by user on 2017-12-23.
 */

public class PTGPUGLSurfaceView extends GLSurfaceView {
    PTGPURenderer pgr;

    public PTGPUGLSurfaceView(Context context) {
        super(context);

        pgr = new PTGPURenderer();
        setRenderer(pgr);
    }
}
