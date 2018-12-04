package gamemobile.kmu.ac.kr.ptgpu;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.View;

import static android.view.MotionEvent.ACTION_DOWN;
import static android.view.MotionEvent.ACTION_UP;

/**
 * Created by user on 2017-12-23.
 */

public class PTGPUGLSurfaceView extends GLSurfaceView {
    int touchX, touchY;
    PTGPURenderer pgr;

    public PTGPUGLSurfaceView(Context context) {
        super(context);

        pgr = new PTGPURenderer(context);
        setRenderer(pgr);

        setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == ACTION_DOWN) {
                    touchX = (int) event.getX();
                    touchY = (int) event.getY();
                }
                else if (event.getAction() == ACTION_UP) {
                    pgr.onTouch(touchX - (int) event.getX(), touchY - (int) event.getY());
                }

                return true;
            };
        });
    }
}
