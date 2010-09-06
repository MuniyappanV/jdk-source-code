/*
 * @(#)PackageSummaryBuilder.java	1.7 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.doclets.internal.toolkit.builders;

import com.sun.tools.doclets.internal.toolkit.util.*;
import com.sun.tools.doclets.internal.toolkit.*;
import com.sun.javadoc.*;
import java.io.*;
import java.util.*;
import java.lang.reflect.*;

/**
 * Builds the summary for a given package.
 *
 * This code is not part of an API.
 * It is implementation that is subject to change.
 * Do not use it as an API
 * 
 * @author Jamie Ho
 * @since 1.5
 */
public class PackageSummaryBuilder extends AbstractBuilder {

	/**
	 * The root element of the package summary XML is {@value}.
	 */
	public static final String ROOT = "PackageDoc";

	/**
	 * The package being documented.
	 */
	private PackageDoc packageDoc;

	/**
	 * The doclet specific writer that will output the result.
	 */
	private PackageSummaryWriter packageWriter;

	private PackageSummaryBuilder(Configuration configuration) {
		super(configuration);
	}

	/**
	 * Construct a new PackageSummaryBuilder.
	 * @param configuration the current configuration of the doclet.
	 * @param pkg the package being documented.
	 * @param packageWriter the doclet specific writer that will output the 
	 *        result.
	 *
	 * @return an instance of a PackageSummaryBuilder.
	 */
	public static PackageSummaryBuilder getInstance(
		Configuration configuration,
		PackageDoc pkg,
		PackageSummaryWriter packageWriter) {
		PackageSummaryBuilder builder =
			new PackageSummaryBuilder(configuration);
		builder.packageDoc = pkg;
		builder.packageWriter = packageWriter;
		return builder;
	}

	/**
	 * {@inheritDoc}
	 */
	public void invokeMethod(
		String methodName,
		Class[] paramClasses,
		Object[] params)
		throws Exception {
		if (DEBUG) {
			configuration.root.printError(
				"DEBUG: " + this.getClass().getName() + "." + methodName);
		}
		Method method = this.getClass().getMethod(methodName, paramClasses);
		method.invoke(this, params);
	}

	/**
	 * Build the package summary.
	 */
	public void build() throws IOException {
		if (packageWriter == null) {
			//Doclet does not support this output.
			return;
		}
		build(LayoutParser.getInstance(configuration).parseXML(ROOT));
	}

	/**
	 * {@inheritDoc}
	 */
	public String getName() {
		return ROOT;
	}

	/**
	 * Build the package documentation.
	 */
	public void buildPackageDoc(List elements) throws Exception {
		build(elements);
		packageWriter.close();
		Util.copyDocFiles(
			configuration,
			Util.getPackageSourcePath(configuration, packageDoc),
			DirectoryManager.getDirectoryPath(packageDoc)
				+ File.separator
				+ DocletConstants.DOC_FILES_DIR_NAME,
			true);
	}

	/**
	 * Build the header of the summary.
	 */
	public void buildPackageHeader() {
		packageWriter.writePackageHeader(Util.getPackageName(packageDoc));
	}

	/**
	 * Build the description of the summary.
	 */
	public void buildPackageDescription() {
		if (configuration.nocomment) {
			return;
		}
		packageWriter.writePackageDescription();
	}

	/**
	 * Build the tags of the summary.
	 */
	public void buildPackageTags() {
		if (configuration.nocomment) {
			return;
		}
		packageWriter.writePackageTags();
	}

	/**
	 * Build the package summary.
	 */
	public void buildSummary(List elements) {
		build(elements);
	}

	/**
	 * Build the overall header.
	 */
	public void buildSummaryHeader() {
		packageWriter.writeSummaryHeader();
	}

	/**
	 * Build the overall footer.
	 */
	public void buildSummaryFooter() {
		packageWriter.writeSummaryFooter();
	}

	/**
	 * Build the summary for the classes in this package.
	 */
	public void buildClassSummary() {
		ClassDoc[] classes =
			packageDoc.isIncluded()
				? packageDoc.ordinaryClasses()
				: configuration.classDocCatalog.ordinaryClasses(
					Util.getPackageName(packageDoc));
		if (classes.length > 0) {
			packageWriter.writeClassesSummary(
				classes,
				configuration.getText("doclet.Class_Summary"));
		}
	}

	/**
	 * Build the summary for the interfaces in this package.
	 */
	public void buildInterfaceSummary() {
		ClassDoc[] interfaces =
			packageDoc.isIncluded()
				? packageDoc.interfaces()
				: configuration.classDocCatalog.interfaces(
					Util.getPackageName(packageDoc));
		if (interfaces.length > 0) {
			packageWriter.writeClassesSummary(
				interfaces,
				configuration.getText("doclet.Interface_Summary"));
		}
	}

	/**
	 * Build the summary for the enums in this package.
	 */
	public void buildAnnotationTypeSummary() {
		ClassDoc[] annotationTypes =
			packageDoc.isIncluded()
				? packageDoc.annotationTypes()
				: configuration.classDocCatalog.annotationTypes(
					Util.getPackageName(packageDoc));
		if (annotationTypes.length > 0) {
			packageWriter.writeClassesSummary(
				annotationTypes,
				configuration.getText("doclet.Annotation_Types_Summary"));
		}
	}

	/**
	 * Build the summary for the enums in this package.
	 */
	public void buildEnumSummary() {
		ClassDoc[] enums =
			packageDoc.isIncluded()
				? packageDoc.enums()
				: configuration.classDocCatalog.enums(
					Util.getPackageName(packageDoc));
		if (enums.length > 0) {
			packageWriter.writeClassesSummary(
				enums,
				configuration.getText("doclet.Enum_Summary"));
		}
	}

	/**
	 * Build the summary for the exceptions in this package.
	 */
	public void buildExceptionSummary() {
		ClassDoc[] exceptions =
			packageDoc.isIncluded()
				? packageDoc.exceptions()
				: configuration.classDocCatalog.exceptions(
					Util.getPackageName(packageDoc));
		if (exceptions.length > 0) {
			packageWriter.writeClassesSummary(
				exceptions,
				configuration.getText("doclet.Exception_Summary"));
		}
	}

	/**
	 * Build the summary for the errors in this package.
	 */
	public void buildErrorSummary() {
		ClassDoc[] errors =
			packageDoc.isIncluded()
				? packageDoc.errors()
				: configuration.classDocCatalog.errors(
					Util.getPackageName(packageDoc));
		if (errors.length > 0) {
			packageWriter.writeClassesSummary(
				errors,
				configuration.getText("doclet.Error_Summary"));
		}
	}

	/**
	 * Build the footer of the summary.
	 */
	public void buildPackageFooter() {
		packageWriter.writePackageFooter();
	}
}
