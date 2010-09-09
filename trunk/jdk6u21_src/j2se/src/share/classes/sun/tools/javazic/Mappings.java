/*
 * @(#)Mappings.java	1.9 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.tools.javazic;

import	java.util.ArrayList;
import	java.util.HashMap;
import	java.util.HashSet;
import	java.util.Iterator;
import	java.util.LinkedList;
import	java.util.List;
import	java.util.Map;
import	java.util.Set;
import	java.util.TreeMap;
import	java.util.TreeSet;

/**
 * <code>Mappings</code> generates two Maps and a List which are used by
 * javazic BackEnd.
 *
 * @since 1.4
 */
class Mappings {
    // All aliases specified by Link statements. It's alias name to
    // real name mappings.
    private Map<String,String> aliases;

    private List<Integer> rawOffsetsIndex;

    private List<Set<String>> rawOffsetsIndexTable;

    // Zone names to be excluded from rawOffset table. Those have GMT
    // offsets to change some future time.
    private List<String> excludeList;

    /**
     * Constructor creates some necessary instances.
     */
    Mappings() {
	aliases = new TreeMap<String,String>();
	rawOffsetsIndex = new LinkedList<Integer>();
	rawOffsetsIndexTable = new LinkedList<Set<String>>();
    }
 
    /**
     * Generates aliases and rawOffsets tables.
     * @param zi a Zoneinfo containing Zones
     */
    void add(Zoneinfo zi) {
	Map<String,Zone> zones = zi.getZones();

	for (String zoneName : zones.keySet()) {
	    Zone zone = zones.get(zoneName);
	    String zonename = zone.getName();
	    int rawOffset = zone.get(zone.size()-1).getGmtOffset();
	    // If the GMT offset of this Zone will change in some
	    // future time, this Zone is added to the exclude list.
	    boolean isExcluded = false;
	    if (zone.size() > 1) {
		ZoneRec zrec = zone.get(zone.size()-2);
		if ((zrec.getGmtOffset() != rawOffset)
		    && (zrec.getUntilTime(0) > Time.getCurrentTime())) {
		    if (excludeList == null) {
			excludeList = new ArrayList<String>();
		    }
		    excludeList.add(zone.getName());
		    isExcluded = true;
		}
	    }

	    if (!rawOffsetsIndex.contains(new Integer(rawOffset))) {
		// Find the index to insert this raw offset zones
		int n = rawOffsetsIndex.size();
		int i;
		for (i = 0; i < n; i++) {
		    if (rawOffsetsIndex.get(i) > rawOffset) {
			break;
		    }
		}
		rawOffsetsIndex.add(i, rawOffset);

		Set<String> perRawOffset = new TreeSet<String>();
		if (!isExcluded) {
		    perRawOffset.add(zonename);
		}
		rawOffsetsIndexTable.add(i, perRawOffset);
	    } else if (!isExcluded) {
		int i = rawOffsetsIndex.indexOf(new Integer(rawOffset));
		Set<String> perRawOffset = rawOffsetsIndexTable.get(i);
		perRawOffset.add(zonename);
	    }
	}

	Map<String,String> a = zi.getAliases();
	// If there are time zone names which refer to any of the
	// excluded zones, add those names to the excluded list.
	if (excludeList != null) {
	    for (String zoneName : a.keySet()) {
		String realname = a.get(zoneName);
		if (excludeList.contains(realname)) {
		    excludeList.add(zoneName);
		}
	    }
	}
	aliases.putAll(a);
    }

    /**
     * Adds valid aliases to one of per-RawOffset table and removes
     * invalid aliases from aliases List. Aliases referring to
     * excluded zones are not added to a per-RawOffset table.
     */
    void resolve() {
	int index = rawOffsetsIndexTable.size();
	List<String> toBeRemoved = new ArrayList<String>();
	for (String key : aliases.keySet()) {
	    boolean validname = false;
	    for (int j = 0; j < index; j++) {
		Set<String> perRO = rawOffsetsIndexTable.get(j);
		boolean isExcluded = (excludeList == null) ?
					false : excludeList.contains(key);

		if ((perRO.contains(aliases.get(key)) || isExcluded)
		    && Zone.isTargetZone(key)) {
		    validname = true;
		    if (!isExcluded) {
			perRO.add(key);
			Main.info("Alias <"+key+"> added to the list.");
		    }
		    break;
		}
	    } 

	    if (!validname) {
		Main.info("Alias <"+key+"> removed from the list.");
		toBeRemoved.add(key);
	    }
	}

	// Remove zones, if any, from the list.
	for (String key : toBeRemoved) {
	    aliases.remove(key);
	}
    }

    Map<String,String> getAliases() {
	return(aliases);
    } 

    List<Integer> getRawOffsetsIndex() {
	return(rawOffsetsIndex);
    }

    List<Set<String>> getRawOffsetsIndexTable() {
	return(rawOffsetsIndexTable);
    }

    List<String> getExcludeList() {
	return excludeList;
    }
}
