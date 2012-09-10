<?php

// Read a NagMap configuration file
include('nagios_cfg.php');

// Get list of all files with Nagios configuration into an array
$files = get_config_files();
$info_msg["path_files_to_read"] = $files;

// Read all Nagios configuration files into one huge array
foreach ($files as $file) {
  $raw_data[$file] = file($file);
}

include("status.php");

//pre-define variables so the E_NOTICES do not show in webserver logs
$javascript = "";

$s = nagmap_status();
$info_msg['status'] = $s;

$i=0; 
foreach ($raw_data as $file) {
 foreach ($file as $line) {
  //remove blank spaces
  $line = trim($line);
  if ($line && !preg_match("/^;/", $line) && !preg_match("/^#/", $line)) {
    //replace many spaces with just one (or tab to one space)
    $line = preg_replace('/\s+/', ' ', $line);
    $line = preg_replace('/\t+/', ' ', $line);
    if ((preg_match("/^define host{/", $line)) OR (preg_match("/^define host {/", $line)) OR (preg_match("/^define hostextinfo {/", $line)) OR (preg_match("/^define hostextinfo{/", $line))) {
      //starting a new host definition
      $i++;
    } elseif (!preg_match("/}/",$line)) {
      //split line to options and values
      $pieces = explode(" ", $line, 2);
      //get rid of meaningless splits
      if (count($pieces)<2) { continue; };
      $option = trim($pieces[0]);
      $value = trim($pieces[1]);
      //remove comments from this line
      $value_comm = explode(';', $value);
      $data[$i][$option] = $value_comm[0];
    }
  }
 }
}
unset($i);

//hosts definition
foreach ($data as $host) {
  if (!empty($host["host_name"])) {
    $nagios_host_name = $host["host_name"];
    $hostname = trim($host["host_name"]);
    $hostname = str_replace('-','_',$hostname);
    $hostname = str_replace('.','_',$hostname);
    $hostname = str_replace('/','_',$hostname);
    $hostname = str_replace('(','_',$hostname);
    $hostname = str_replace(')','_',$hostname);
    $hostname = str_replace(' ','_',$hostname);
  }
  //if hostname is empty or hostname starts with exclamation mark, ignore this host
  if (empty($hostname) OR (preg_match("/^\\!/", $hostname)) ) {
    continue;
  };
  $hostname = "x".$hostname."x";
  $host["host_name"] = $hostname;
  
  foreach ($host as $option => $value) {
    if ($option == "parents") {
      $value = trim($value);
      $value = str_replace('-','_',$value);
      $value = str_replace('.','_',$value);
      $value = str_replace('/','_',$value);
      $value = str_replace('(','_',$value);
      $value = str_replace(')','_',$value);
      $parents = explode(',', $value); 
      $value = array();
      foreach ($parents as $parent) {
        $parent = trim($parent);
        $parent = str_replace(' ','_',$parent);
        $value[] = "x".$parent."x";
      }
    }
    if (($option == "notes") && (preg_match("/latlng/",$value))) { 
      $value = explode(":",$value); 
      $value = $value[1];
      $value = trim($value);
      $option = "latlng";
    };
    if (($option != "latlng") && ($option != "nagios_host_name") && ($option != "parents") && (preg_match("/-/",$value))) {
      $value = str_replace('-','_',$value);
      $value = str_replace('.','_',$value);
      $value = trim($value);
    };
    $hosts[$hostname]["nagios_host_name"] = $nagios_host_name;
    $hosts[$hostname][$option] = $value;
    unset($parent, $parents);
  };
};

$info_msg['hosts'] = $hosts;

//pre-define variables so we do not get warnings in webserver logs
$sidebar['ok'] = Array();
$sidebar['critical'] = Array();
$sidebar['warning'] = Array();
$sidebar['unknown'] = Array();
$stats['ok'] = 0;
$stats['critical'] = 0;
$stats['warning'] = 0;
$stats['unknown'] = 0;

//put markers and bubbles onto a map
foreach ($hosts as $h) {
  if ((isset($h["latlng"])) and (isset($h["host_name"]))) {
    // position the host to the map
    $javascript .= ("window.".$h["host_name"]."_pos = new google.maps.LatLng(".$h["latlng"].");\n");

    // display different icons for the host (according to the status in nagios)
    // if host is in state OK and it is a special type of host (for wifi hotspot networks)
    if (($h["use"] == "wifi_hotspot") && ($s[$h["nagios_host_name"]]['status'] == 0)) {
      $javascript .= ('window.'.$h["host_name"]."_mark = new google.maps.Marker({".
        "\n  position: ".$h["host_name"]."_pos,".
        "\n  icon: 'http://www.google.com/mapfiles/marker_white.png',".
        "\n".'  '."map: map,".
	"\n  zIndex: 1,".
        "\n  title: \"".$h["nagios_host_name"]."\"".
        "\n  });"."\n\n");
        $stats['ok']++;
        $sidebar['ok'][] = '<a href="javascript:'.$h["host_name"].'_mark_infowindow.open(map,'.$h["host_name"].'_mark)" class="'.$s[$h["nagios_host_name"]]['status_style'].'">'.$h["nagios_host_name"]."</a><br>\n";
    // if host is in state OK
    } elseif ($s[$h["nagios_host_name"]]['status'] == 0) {
      $javascript .= ('window.'.$h["host_name"]."_mark = new google.maps.Marker({".
        "\n  position: ".$h["host_name"]."_pos,".
        "\n  icon: 'http://www.google.com/mapfiles/marker_green.png',".
        "\n  map: map,".
        "\n  zIndex: 2,".
        "\n  title: \"".$h["nagios_host_name"]."\"".
        "});"."\n\n");
        $stats['ok']++;
        $sidebar['ok'][] = '<a href="javascript:'.$h["host_name"].'_mark_infowindow.open(map,'.$h["host_name"].'_mark)" class="'.$s[$h["nagios_host_name"]]['status_style'].'">'.$h["nagios_host_name"]."</a><br>\n";
    // if host is in state WARNING 
    } elseif ($s[$h["nagios_host_name"]]['status'] == 1) {
      $javascript .= ('window.'.$h["host_name"]."_mark = new google.maps.Marker({".
        "\n  position: ".$h["host_name"]."_pos,".
        "\n  icon: 'http://www.google.com/mapfiles/marker_yellow.png',".
        "\n  map: map,".
        "\n  zIndex: 3,".
        "\n  title: \"".$h["nagios_host_name"]."\"".
        "});"."\n\n");
        $stats['warning']++;
        $sidebar['warning'][] = '<a href="javascript:'.$h["host_name"].'_mark_infowindow.open(map,'.$h["host_name"].'_mark)" class="'.$s[$h["nagios_host_name"]]['status_style'].'">'.$h["nagios_host_name"]."</a><br>\n";
    // if host is in state CRITICAL / UNREACHABLE
    } elseif ($s[$h["nagios_host_name"]]['status'] == 2) {
      $javascript .= ('window.'.$h["host_name"]."_mark = new google.maps.Marker({".
        "\n  position: ".$h["host_name"]."_pos,".
        "\n  icon: 'http://www.google.com/mapfiles/marker.png',".
        "\n  map: map,".
        "\n  zIndex: 4,".
        "\n  title: \"".$h["nagios_host_name"]."\"".
        "});"."\n\n");
        $stats['critical']++;
        $sidebar['critical'][] = '<a href="javascript:'.$h["host_name"].'_mark_infowindow.open(map,'.$h["host_name"].'_mark)" class="'.$s[$h["nagios_host_name"]]['status_style'].'">'.$h["nagios_host_name"]."</a><br>\n";
    // if host is in state UNKNOWN
    } elseif ($s[$h["nagios_host_name"]]['status'] == 3) {
      $javascript .= ('window.'.$h["host_name"]."_mark = new google.maps.Marker({".
        "\n  position: ".$h["host_name"]."_pos,".
        "\n  icon: 'http://www.google.com/mapfiles/marker_grey.png',".
        "\n  map: map,".
        "\n  zIndex: 2,".
        "\n  title: \"".$h["nagios_host_name"]."\"".
        "});"."\n\n");
        $stats['unknown']++;
        $sidebar['unknown'][] = '<a href="javascript:'.$h["host_name"].'_mark_infowindow.open(map,'.$h["host_name"].'_mark)" class="'.$s[$h["nagios_host_name"]]['status_style'].'">'.$h["nagios_host_name"]."</a><br>\n";
    } else {
    // if host is in any other (unknown to nagmap) state
      $javascript .= ('window.'.$h["host_name"]."_mark = new google.maps.Marker({".
        "\n  position: ".$h["host_name"]."_pos,".
        "\n  icon: 'http://www.google.com/mapfiles/marker_grey.png',".
        "\n  map: map,".
        "\n  zIndex: 6,".
        "\n  title: \"".$h["nagios_host_name"]."\"".
        "});"."\n\n");
    };
    //generate google maps info bubble
    if (!isset($h["parents"])) { $h["parents"] = Array(); }; 
    $info = '<div class=\"bubble\"><b>'.$h["nagios_host_name"]."</b><br>Type: ".$h["use"]
         .'<br>Address:'.$h["address"]
         .'<br>Number of parents:'.count($h["parents"]).','
         .'<br>Host status: '.$s[$h["nagios_host_name"]]["hoststatus"]["last_hard_state"]
         //.'<br>Services status: '.$s[$h["nagios_host_name"]]["servicestatus"]["last_hard_state"]
         .'<br>Combined / NagMap status: '.$s[$h["nagios_host_name"]]['status'].' : '.$s[$h["nagios_host_name"]]['status_human']
         .'<br><a href=\"/nagios/cgi-bin/statusmap.cgi\?host='.$h["nagios_host_name"].'\">Nagios map page</a>'
         .'<br><a href=\"/nagios/cgi-bin/extinfo.cgi\?type=1\&host='.$h["nagios_host_name"].'\">Nagios host page</a>';
    $links = '<br><a href=\"../cgi-bin/smokeping.cgi?target=LAN.'.$h["nagios_host_name"].'\">Smokeping statistics</a>'
         .'<br><a href=\"../devices/modules/mrtg_uptime/workdir/'.$h["nagios_host_name"].'.html\">Uptime Graph</a>';
    if ($nagmap_bubble_links == 1) {
      $info = $info.$links;
    } 
    $info = $info.'<br><span style=\"font-size: 7pt\">NagMap by blava.net</span></div>';

    $javascript .= ("window.".$h["host_name"]."_mark_infowindow = new google.maps.InfoWindow({ content: '$info'})\n");

    $javascript .= ("google.maps.event.addListener(".$h["host_name"]."_mark, 'click', function() {"
      .$h["host_name"]."_mark_infowindow.open(map,".$h["host_name"]."_mark);\n
      });\n\n");

  };
};

//create (multiple) parent connection links between nodes/markers
$javascript .= "//generating links between hosts\n";
foreach ($hosts as $h) {
  if (!isset($h["parents"])) { $h["parents"] = Array(); };
  if (isset($h["latlng"]) AND (is_array($h["parents"]))) {
    foreach ($h["parents"] as $parent) {
      if (isset($hosts[$parent]["latlng"])) {
        // default colors for links
        $stroke_color = "#ADDFFF";
	// links in warning state
        if ($s[$h["nagios_host_name"]]['status'] == 1) { $stroke_color ='#ffff00'; }
	//links in problem state
        if ($s[$h["nagios_host_name"]]['status'] == 2) { $stroke_color ='#ff0000'; }

        $javascript .= ("\nwindow.".$h["host_name"].'_to_'.$parent." = new google.maps.Polyline({\n".
          "  path: [".$h["host_name"].'_pos,'.$parent."_pos],\n".
          "  strokeColor: \"$stroke_color\",\n".
          "  strokeOpacity: 0.9,\n".
          "  strokeWeight: 2});\n");
        $javascript .= ($h["host_name"].'_to_'.$parent.".setMap(map);\n\n");
      };
    };
  };
};

?>
