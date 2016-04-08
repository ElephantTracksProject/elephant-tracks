/*
 * Based on DacapoClassLoader, part of the DaCapo benchmark suite,
 * which comes with this license:
 *
 * Copyright (c) 2006, 2009 The Australian National University.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Apache License v2.0.
 * You may obtain the license at
 * 
 *    http://www.opensource.org/licenses/apache2.0.php
 */
package classReWriter;

// NOTE: This class is not used at present; see comments in ETClassWriter.java

import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;

/**
 * Custom class loader for ElephantTracks to use in cases such as the
 * DaCapo benchmarks.  Instances of this classloader
 * are created by passing a list of jar files.
 */
public class ETClassLoader extends URLClassLoader {

  /**
   * Factory method to create a class loader
   * 
   * @param jars an array of String giving the names of the jar files
   * that this classloader will search, <i>before</i> searching the
   * usual Java class path
   * @return The new class loader
   * @throws Exception
   */
  public static ETClassLoader create (String[] jars) {
    ETClassLoader rtn = null;
    try {
      URL[] urls = new URL[jars.length];
      for (int i = 0; i < jars.length; ++i) {
        File ajar = new File(jars[i]);
        urls[i] = ajar.toURI().toURL();
      }
      rtn = new ETClassLoader(urls, Thread.currentThread().getContextClassLoader());
    } catch (Exception e) {
      System.err.println("Unable to create ETClassLoader:");
      e.printStackTrace();
      System.exit(-1);
    }
    return rtn;
  }

  /**
   * @param urls
   * @param parent
   */
  private ETClassLoader(URL[] urls, ClassLoader parent) {
    super(urls, parent);
  }

  /**
   * Reverse the logic of the default classloader, by trying the top-level
   * classes first. This way, libraries packaged with the benchmarks override
   * those provided by the runtime environment.
   */
  @Override
  protected synchronized Class<?> loadClass (String name, boolean resolve) throws ClassNotFoundException {
    // First, check if the class has already been loaded
    Class<?> c = findLoadedClass(name);
    if (c == null) {
      try {
        // Next, try to resolve it from our given jar files
        c = super.findClass(name);
      } catch (ClassNotFoundException e) {
        // And if all else fails delegate to the parent.
        c = super.loadClass(name, resolve);
      }
    }
    if (resolve) {
      resolveClass(c);
    }
    return c;
  }
}

// Local Variables:
// mode:Java
// c-basic-offset:2
// indent-tabs-mode:nil
// End:
