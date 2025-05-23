/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.facebook.jni;

import com.facebook.infer.annotation.Nullsafe;
import com.facebook.soloader.nativeloader.NativeLoader;
import com.facebook.soloader.nativeloader.SystemDelegate;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.rules.ExpectedException;

@Nullsafe(Nullsafe.Mode.LOCAL)
public class BaseFBJniTests {
  // NULLSAFE_FIXME[Not Vetted Third-Party]
  @Rule public ExpectedException thrown = ExpectedException.none();

  @BeforeClass
  public static void setup() {
    if (!NativeLoader.isInitialized()) {
      NativeLoader.init(new SystemDelegate());
    }
    // Explicitly load fbjni to ensure that its JNI_OnLoad is run.
    NativeLoader.loadLibrary("fbjni");
    NativeLoader.loadLibrary("fbjni-tests");
  }
}
