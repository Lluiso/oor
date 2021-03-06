/*
 *
 * Copyright (C) 2011, 2015 Cisco Systems, Inc.
 * Copyright (C) 2015 CBA research group, Technical University of Catalonia.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package org.openoverlayrouter.noroot;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;


public class OOR extends Fragment {

    protected static SuShell shell;
    public static String oor_path = "";
    private boolean oorWasRunning = false;
    private static boolean oorRunning = false;
    public static String conf_file = "";
    public static String prefix = "";
    private static final int CONF_ACT = 1;
    private LinearLayout llLayout;
    private FragmentActivity faActivity;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {

        faActivity = (FragmentActivity) super.getActivity();
        llLayout = (LinearLayout) inflater.inflate(R.layout.main, container, false);


        try {
            shell = new SuShell();
        } catch (IOException e) {
        }

		/* Get the directory of the executable */

        try {
            oor_path = faActivity.getPackageManager().getApplicationInfo("org.openoverlayrouter", 0).nativeLibraryDir; //  dataDir + "/lib"
            conf_file = Environment.getExternalStorageDirectory().getAbsolutePath() + "/oor.conf";
        } catch (Exception e) {
            Log.e("OOR", e.getMessage());
        }


        prefix = faActivity.getPackageName();

        handler = new Handler();
        doUpdateView = new Runnable() {
            public void run() {
                updateStatus();
            }
        };

        ImageButton oor = (ImageButton) llLayout.findViewById(R.id.oorStart);
        oor.setOnClickListener(new OnClickListener() {
                                   public void onClick(View view) {
                                       if (oorRunning == false) {
                                           File file = new File(conf_file);
                                           if (!file.exists()) {
                                               showMessage(faActivity.getString(R.string.noConfFile), false, null);
                                           }
                                           startOOR();
                                       } else {
                                           showMessage(faActivity.getString(R.string.askStopServiceString),
                                                   true, new Runnable() {
                                                       public void run() {
                                                           killOOR();
                                                           oorWasRunning = false;
                                                           oorRunning = false;
                                                       }
                                                   });
                                       }


                                   }
                               }
        );

        return llLayout;
    }


    @Override
    public void onPause() {
        super.onPause();

        Log.v("OOR", "Pausing..");

    }

    @Override
    public void onStop() {
        super.onStop();

        Log.v("OOR", "Stopping...");


    }

    @Override
    public void onResume() {
        super.onResume();

        Log.v("OOR", "Resuming...");

    }


    public void updateStatus() {
        final TextView oorCheckBoxLabel = (TextView) llLayout.findViewById(R.id.startStopCheckboxLabel);
        final TextView oorTextClick = (TextView) llLayout.findViewById(R.id.textClick);


        if (oorRunning) {
            oorTextClick.setText("Click on the icon to stop the service");
            oorCheckBoxLabel.setText(R.string.oorRunning);
            oorCheckBoxLabel.setTextColor(Color.BLACK);
            oorWasRunning = true;
        } else {
            if (oorWasRunning) {
                oorTextClick.setText("Click on the icon to restart the service");
                oorCheckBoxLabel.setText("OOR has exited");
                oorCheckBoxLabel.setTextColor(Color.RED);
            } else {
                oorTextClick.setText("Click on the icon to start the service");
                oorCheckBoxLabel.setText(R.string.oorNotRunning);
                oorCheckBoxLabel.setTextColor(Color.BLACK);
            }
        }
    }

    public static boolean isOorRunning() {
        return (OORService.isRunning);
    }

    @Override
    public void onStart() {
        super.onStart();

    }



    public void showMessage(String message, boolean cancelAble, final Runnable task) {

        AlertDialog.Builder builder = new AlertDialog.Builder(faActivity);
        builder.setTitle("Attention:");
        builder.setMessage(message)
                .setCancelable(cancelAble)
                .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        if (task != null) {
                            task.run();
                        } else {
                            dialog.dismiss();
                        }
                    }
                });
        if (cancelAble) {
            builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    dialog.dismiss();
                }
            });
        }
        AlertDialog alert = builder.create();
        alert.show();
    }

    public void startOOR() {
        String command = oor_path + "/liboorexec.so -D -d 2 -f " + conf_file;
        shell.run_no_output(command);

        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        Intent serviceIntent = new Intent(faActivity, OORService.class);
        serviceIntent.putExtra(prefix + ".START", true);
        faActivity.startService(serviceIntent);
        oorRunning = true;
        updateStatus();
    }

    public void killOOR() {
        String psOutput = shell.run("/system/bin/ps | grep liboorexec.so");
        Boolean isRunning = psOutput.matches("(?s)(.*)[RS]\\s[a-zA-Z0-9\\/\\.\\-]*liboorexec\\.so(.*)");
        if (isRunning) {
            String[] split = psOutput.split("\\s+");
            String command = "kill " + split[1];
            shell.run_no_output(command);
        }

        oorWasRunning = false;

        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        Intent serviceIntent = new Intent(faActivity, OORService.class);
        serviceIntent.putExtra(prefix + ".START", false);
        faActivity.startService(serviceIntent);
        updateStatus();
    }

    public void restartOOR() {
        System.out.println("OOR: Restarting oor");
        killOOR();
        startOOR();
    }

    Handler handler;
    Runnable doUpdateView;


    public void onActivityResult(int request, int result, Intent data) {
        switch (request) {
            case CONF_ACT:
                if (result == updateConfActivity.CONFIG_UPDATED) {
                    if (isOorRunning()) {
                        restartOOR();
                    }
                }
                break;
            default:
                break;
        }
    }



}

