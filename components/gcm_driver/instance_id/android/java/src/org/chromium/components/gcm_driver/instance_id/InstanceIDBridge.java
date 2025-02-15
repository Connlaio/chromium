// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;

import com.google.android.gms.iid.InstanceID;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/**
 * Wraps InstanceID and InstanceIDWithSubtype so they can be used over JNI.
 * Performs disk/network operations on a background thread and replies asynchronously.
 */
@JNINamespace("instance_id")
public class InstanceIDBridge {
    /** Underlying InstanceID. May be shared by multiple InstanceIDBridges. */
    private final InstanceID mInstanceID;
    private long mNativeInstanceIDAndroid;

    private static boolean sBlockOnAsyncTasksForTesting = false;

    private InstanceIDBridge(
            long nativeInstanceIDAndroid, Context context, String subtype) {
        mInstanceID = InstanceIDWithSubtype.getInstance(context, subtype);
        mNativeInstanceIDAndroid = nativeInstanceIDAndroid;
    }

    /**
     * Returns a wrapped {@link InstanceIDWithSubtype}. Multiple InstanceIDBridge instances may
     * share an underlying InstanceIDWithSubtype.
     */
    @CalledByNative
    public static InstanceIDBridge create(
            long nativeInstanceIDAndroid, Context context, String subtype) {
        // TODO(johnme): This should also be async.
        return new InstanceIDBridge(nativeInstanceIDAndroid, context, subtype);
    }

    /**
     * Called when our C++ counterpart is destroyed. Clears the handle to our native C++ object,
     * ensuring it's not called by pending async tasks.
     */
    @CalledByNative
    private void destroy() {
        mNativeInstanceIDAndroid = 0;
    }

    /**
     * If enabled, methods that are usually asynchronous will block returning the control flow to
     * C++ until the asynchronous Java operation has completed. Used this in tests where the Java
     * message loop is not nested. The caller is expected to reset this to false when tearing down.
     */
    @CalledByNative
    private static boolean setBlockOnAsyncTasksForTesting(boolean block) {
        boolean wasBlocked = sBlockOnAsyncTasksForTesting;
        sBlockOnAsyncTasksForTesting = block;
        return wasBlocked;
    }

    /** Wrapper for {@link InstanceID#getId}. */
    @CalledByNative
    public String getId() {
        // TODO(johnme): This should also be async.
        return mInstanceID.getId();
    }

    /** Wrapper for {@link InstanceID#getCreationTime}. */
    @CalledByNative
    public long getCreationTime() {
        // TODO(johnme): This should also be async.
        return mInstanceID.getCreationTime();
    }

    /** Async wrapper for {@link InstanceID#getToken(String, String, Bundle)}. */
    @CalledByNative
    private void getToken(final int requestId, final String authorizedEntity, final String scope,
            String[] extrasStrings) {
        final Bundle extras = new Bundle();
        assert extrasStrings.length % 2 == 0;
        for (int i = 0; i < extrasStrings.length; i += 2) {
            extras.putString(extrasStrings[i], extrasStrings[i + 1]);
        }
        new BridgeAsyncTask<String>() {
            @Override
            protected String doBackgroundWork() {
                try {
                    return mInstanceID.getToken(authorizedEntity, scope, extras);
                } catch (IOException ex) {
                    return "";
                }
            }
            @Override
            protected void sendResultToNative(String token) {
                nativeDidGetToken(mNativeInstanceIDAndroid, requestId, token);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteToken(String, String)}. */
    @CalledByNative
    private void deleteToken(
            final int requestId, final String authorizedEntity, final String scope) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork() {
                try {
                    mInstanceID.deleteToken(authorizedEntity, scope);
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }
            @Override
            protected void sendResultToNative(Boolean success) {
                nativeDidDeleteToken(mNativeInstanceIDAndroid, requestId, success);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteInstanceID}. */
    @CalledByNative
    private void deleteInstanceID(final int requestId) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork() {
                try {
                    mInstanceID.deleteInstanceID();
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }
            @Override
            protected void sendResultToNative(Boolean success) {
                nativeDidDeleteID(mNativeInstanceIDAndroid, requestId, success);
            }
        }.execute();
    }

    private native void nativeDidGetToken(
            long nativeInstanceIDAndroid, int requestId, String token);
    private native void nativeDidDeleteToken(
            long nativeInstanceIDAndroid, int requestId, boolean success);
    private native void nativeDidDeleteID(
            long nativeInstanceIDAndroid, int requestId, boolean success);

    /**
     * Custom {@link AsyncTask} wrapper. As usual, this performs work on a background thread, then
     * sends the result back on the UI thread. There are 3 differences:
     * 1. sendResultToNative will be skipped if the C++ counterpart has been destroyed.
     * 2. Tasks run in parallel (using THREAD_POOL_EXECUTOR) to avoid blocking other Chrome tasks.
     * 3. If setBlockOnAsyncTasksForTesting has been enabled, executing the task will block the UI
     *    thread, then directly call sendResultToNative. This is a workaround for use in tests
     *    that lack a nested Java message loop (which prevents onPostExecute from running).
     */
    private abstract class BridgeAsyncTask<Result> {
        protected abstract Result doBackgroundWork();

        protected abstract void sendResultToNative(Result result);

        public void execute() {
            AsyncTask<Void, Void, Result> task = new AsyncTask<Void, Void, Result>() {
                @Override
                protected Result doInBackground(Void... params) {
                    return doBackgroundWork();
                }
                @Override
                protected void onPostExecute(Result result) {
                    if (!sBlockOnAsyncTasksForTesting && mNativeInstanceIDAndroid != 0) {
                        sendResultToNative(result);
                    }
                }
            };
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            if (sBlockOnAsyncTasksForTesting) {
                Result result;
                try {
                    // Synchronously block the UI thread until doInBackground returns result.
                    result = task.get();
                } catch (InterruptedException | ExecutionException e) {
                    throw new IllegalStateException(e); // Shouldn't happen in tests.
                }
                sendResultToNative(result);
            }
        }
    }
}