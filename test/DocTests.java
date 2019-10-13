// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.jni;

import com.facebook.soloader.nativeloader.NativeLoader;
import org.junit.BeforeClass;
import org.junit.Test;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import static org.fest.assertions.api.Assertions.assertThat;
import static org.fest.assertions.api.Assertions.failBecauseExceptionWasNotThrown;

public class DocTests extends BaseFBJniTests {
  @BeforeClass
  public static void setup() {
    BaseFBJniTests.setup();
    NativeLoader.loadLibrary("doc_tests");
  }

  public String toString() {
    return "instance of DocTests";
  }

  // SECTION basic_methods
  native void nativeVoidMethod();
  static native void staticNativeVoidMethod();
  void voidMethod() {}
  static void staticVoidMethod() {}
  // END

  @Test
  public void testVoids() {
    nativeVoidMethod();
    staticNativeVoidMethod();
  }

  // SECTION primitives
  static native long addSomeNumbers(byte b, short s, int i);
  static long doubler(int i) { return i + i; }
  // END

  @Test
  public void testNumbers() {
    assertThat(addSomeNumbers((byte)1, (short)2, 3)).isEqualTo(14);
  }

  // SECTION strings
  static native String fancyCat(String s1, String s2);
  static native String getCString();
  static String doubler(String s) { return s + s; }
  // END

  @Test
  public void testStrings() {
    assertThat(fancyCat("a", "b")).isEqualTo("aaabbbb");
    assertThat(getCString()).isEqualTo("Watch your memory.");
  }

  // SECTION primitive_arrays
  static native int[] primitiveArrays(int[] arr);
  // END

  @Test
  public void testPrimitiveArrays() {
    assertThat(primitiveArrays(new int[]{1, 2})).contains(1, 2, 3);
  }

  static native Object convertReferences(MyDerivedClass derived);

  @Test
  public void testConvertReferences() {
    MyDerivedClass derived = new MyDerivedClass();
    Object obj = convertReferences(derived);
    assertThat(obj).isSameAs(derived);
  }

  static native void castReferences(MyBaseClass base);

  @Test
  public void testCastReferences() {
    castReferences(new MyDerivedClass());
    try {
      castReferences(new MyBaseClass());
      failBecauseExceptionWasNotThrown(ClassCastException.class);
    } catch (ClassCastException e) {
      assertThat(e).hasMessageContaining("MyBaseClass");
      assertThat(e).hasMessageContaining("MyDerivedClass");
    }
  }

  static native DataHolder runConstructor();

  @Test
  public void testRunConstructor() {
    DataHolder d = runConstructor();
    assertThat(d.i).isEqualTo(1);
    assertThat(d.s).isEqualTo("hi");
  }

  static native void callGetAndSetFields(DataHolder data);

  @Test
  public void testCallGetAndSetFields() {
    synchronized (DataHolder.class) {
      DataHolder dh1 = new DataHolder(1, "1");
      DataHolder dh2 = new DataHolder(3, "3");
      DataHolder.someInstance = null;
      callGetAndSetFields(dh1);
      assertThat(dh1.i).isEqualTo(2);
      assertThat(dh1.s).isEqualTo("11");
      assertThat(DataHolder.someInstance).isSameAs(dh1);
      callGetAndSetFields(dh2);
      assertThat(dh2.i).isEqualTo(4);
      assertThat(dh2.s).isEqualTo("31");
      assertThat(DataHolder.someInstance).isSameAs(dh1);
      DataHolder.someInstance = null;
    }
  }

  static native String showJObject(Object obj, DataHolder data);

  @Test
  public void testShowJObject() {
    String str = showJObject(new Object(), new DataHolder(1, "hi"));
    assertThat(str).startsWith("data=com.facebook.jni.DataHolder@");
  }

  // SECTION boxed
  static native Double scaleUp(Integer number);
  // END

  @Test
  public void testScaleUp() {
    assertThat(scaleUp(5)).isEqualTo(7.5);
  }

  // SECTION iterables
  static native String concatMatches(List<Integer> values, Map<String, Integer> names);
  // END

  @Test
  public void testConcatMatches() {
    Map<String, Integer> names = new TreeMap<>();
    names.put("a", 1);
    names.put("b", 3);
    names.put("c", 3);
    names.put("d", 7);
    assertThat(concatMatches(Arrays.asList(1, 2), names)).isEqualTo("bc");
  }

  static native void catchAndThrow();

  @Test
  public void testCatchAndThrow() {
    try {
      catchAndThrow();
      failBecauseExceptionWasNotThrown(RuntimeException.class);
    } catch (RuntimeException e) {
      assertThat(e)
        .hasMessageStartingWith("Caught 'java.lang.NoSuchMethodError:")
        .hasMessageContaining("doesNotExist")
        ;
    }
  }

  // SECTION byte_buffer
  static native ByteBuffer transformBuffer(ByteBuffer data);
  static void receiveBuffer(ByteBuffer buffer) {
    assertThat(buffer.capacity()).isEqualTo(2);
    assertThat(buffer.get(0)).isEqualTo((byte)2);
    assertThat(buffer.get(1)).isEqualTo((byte)3);
  }
  @Test
  public void testByteBuffers() {
    ByteBuffer data = ByteBuffer.allocateDirect(2);
    data.put(new byte[]{1, 2});
    ByteBuffer transformed = transformBuffer(data);
    receiveBuffer(transformed);
  }
  // END
}

// SECTION inheritance
class MyBaseClass {}
class MyDerivedClass extends MyBaseClass {}
// END

// SECTION nested_class
class Outer {
  class Nested {}
}
// END

// SECTION constructor
class DataHolder {
  int i;
  String s;
  DataHolder(int i, String s) {
    this.i = i;
    this.s = s;
  }
  static DataHolder someInstance;
}
// END
