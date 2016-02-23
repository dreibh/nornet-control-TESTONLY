<?php
//
// NorNet Status Map
// Copyright (C) 2015-2016 by Thomas Dreibholz
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@simula.no
//
?>


<?php
include('NorNet-Status.php');
$status = getNorNetStatus();


// ###### Make contents of the map ##########################################
echo "function makeMapContents() {\n";

// ====== Set up the sites ==================================================
echo "   window.mapContents = new Array();\n";
$sites = 0;
foreach ($status as $hostName => $hostEntry) {
   if (isset($status[$hostName][""]["location"])) {
      // ====== Get location ================================================
      $status[$hostName]['site_number'] = $sites;
      $location = $status[$hostName][""]["city"] . ', ' . $status[$hostName][""]["province"] . '/' . $status[$hostName][""]["country"];
      $image    = '/Artwork/Sites/Large/' . $hostName . '.jpeg';

      echo "   var siteLocation = '" . $status[$hostName][""]["location"] . "';\n";
      echo "   var locationArray = siteLocation.split(',');" . "\n";
      echo "   var latitude  = parseFloat(locationArray[0]);" . "\n";
      echo "   var longitude = parseFloat(locationArray[1]);" . "\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . '] = new Array();' . "\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["name"]          = "' . $hostName . '"' . ";\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["latitude"]      = latitude'  . ";\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["longitude"]     = longitude' . ";\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["city"]          = "' . $status[$hostName][""]["city"] . '"' . ";\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["province"]      = "' . $status[$hostName][""]["province"] . '"' . ";\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["country"]       = "' . $status[$hostName][""]["country"] . '"' . ";\n";
      echo '   window.mapContents[' . $status[$hostName]['site_number'] . ']["country_code"]  = "' . $status[$hostName][""]["country_code"] . '"' . ";\n";

      // ====== Test mode ===================================================
      // echo "latitude  = latitude + ((10 * Math.random()) - 5);" . "\n";
      // echo "longitude = longitude + ((10 * Math.random()) - 5);" . "\n";

      // ====== Create position for sites ===================================
      echo '   // ====== ' . $hostName . ' ======' . "\n";
      echo '   makePosition("window.' . $status[$hostName][""]["host_identifier"] . '_position", latitude, longitude);' . "\n";

      // ====== Create marker ===============================================
      $site       = $hostName;
      $siteStatus = $status[$site][""]["last_hard_state"];
      $icon       = "/Artwork/Graphics/Markers/Marker-Black.svg";
      if (!isset($status[$site][""]['is_disabled_site'])) {
         if ($siteStatus == 0) {
            $icon = "/Artwork/Graphics/Markers/Marker-Green.svg";
         }
         elseif ($siteStatus == 1) {
            $icon = "/Artwork/Graphics/Markers/Marker-Orange.svg";
         }
         elseif ($siteStatus == 2) {
            $icon = "/Artwork/Graphics/Markers/Marker-Red.svg";
         }
      }

      echo '   makeMarker("window.' . $status[$hostName][""]["host_identifier"] . '_marker", ' .
           '"'. $hostName . '", ' .
           '"' . $icon . '", ' .
           '"window.' . $status[$hostName][""]["host_identifier"] . '_position", ' .
           '1, ' .
           "'" . '<h2 class="popup">' . $hostName . '</h2><p class="center">' . $location . '</p><p class="center"><img class="popup" src="' . $image . '" alt="(No image: ' . $image . ')" /></p>' . "'" . ');' . "\n\n";
      $sites++;
   }
}


// ====== Set up the links ==================================================
foreach ($status as $hostName => $hostEntry) {
   foreach ($hostEntry as $serviceName => $serviceEntry) {
      if ($serviceName != "") {   // A real service!
         if ( (isset($status[$hostName][$serviceName]["tunnel_local_host_name"])) &&
              (isset($status[$hostName][$serviceName]["tunnel_remote_host_name"])) ) {
            $tunnelState    = $status[$hostName][$serviceName]['last_hard_state'];
            $localHostName  = $status[$hostName][$serviceName]['tunnel_local_host_name'];
            $remoteHostName = $status[$hostName][$serviceName]['tunnel_remote_host_name'];

            $localIdentifer  = $status[$hostName][$serviceName]['tunnel_local_identifier'];
            $remoteIdentifer = $status[$hostName][$serviceName]['tunnel_remote_identifier'];

            $state = $status[$hostName][$serviceName]['last_hard_state'];

            if( ($state == 1) || ($state == 2) ||
                (isset($status[$hostName][""]['is_central_site'])) ) {

               $zIndex = 10;
               if (!isset($status[$remoteHostName][""]['is_disabled_site'])) {
                  $strokeWeight    = 5;
                  $strokeDashstyle = "solid";
                  switch($state) {
                     case 0:
                        $linkColor = "#00ff00";
                     break;
                     case 1:
                        $linkColor = "yellow";
                        $zIndex    = 15;
                     break;
                     case 2:
                        $linkColor    = "red";
                        $strokeWeight = 10;
                        $zIndex       = 20;
                     break;
                     default:
                        $linkColor = "grey";
                     break;
                  }
               }
               else {
                  $zIndex          = 15;
                  $strokeWeight    = 5;
                  $strokeDashstyle = "dash";
                  $linkColor       = "grey";
               }

               echo "   // ====== Tunnel ".$localHostName." to ".$remoteHostName." S=".$tunnelState." ======\n";
               echo '   makeConnection("window.' . $localIdentifer . '_to_' . $remoteIdentifer . '", ' .
                    '[ ' . $localIdentifer . '_position, window.' . $remoteIdentifer . '_position ], ' .
                    '"' . $linkColor . '", ' .
                    $strokeWeight . ', ' .
                    '"' . $strokeDashstyle . '", ' .
                    '2);' . "\n";
             }
             else {
                echo '   removeConnection("window.' . $localIdentifer . '_to_' . $remoteIdentifer . '");' . "\n";
             }
         }
      }
   }
}

echo "}\n\n";


// ###### Make contents of the sidebar ######################################
echo "function makeSidebarContents() {\n";
$sitesContent = "";
$problems = 0;
$addedProblemsSection = false;
foreach (array('critical', 'warning', 'ok', 'unknown') as $severity) {
   $categoryContent = "";
   foreach ($status as $hostName => $hostEntry) {
      if (isset($status[$hostName][""]["location"])) {
         $site       = $hostName;
         $siteStatus = $status[$hostName][""]["last_hard_state"];
         $flag       = '/Artwork/Graphics/Flags/Flag-' . $status[$hostName][""]["country_code"] . '.png';

         if (isset($status[$hostName][""]['is_disabled_site'])) {
            $siteStatus = 3;
         }

         if( ($severity == "critical") &&
             ($siteStatus == 2) ) {
            $categoryContent = $categoryContent . '<li class="critical"><img class="critical" height="18pt" src="' . $flag . '" alt="" /><a class="critical" href="javascript:void(0);" onclick="zoomToSite(window.mapContents[' . $status[$hostName]['site_number'] . ']);"><blink><strong>&#9760;</strong></blink>' . $site . '</a></li>';
            $problems++;
         }
         elseif( ($severity == "warning") &&
                 ($siteStatus == 1) ) {
            $categoryContent = $categoryContent . '<li class="warning"><img class="warning" height="18pt" src="' . $flag . '" alt="" /><a class="warning" href="javascript:void(0);" onclick="zoomToSite(window.mapContents[' . $status[$hostName]['site_number'] . ']);">&#128544;' . $site . '</a></li>';
            $problems++;
         }
         elseif( ($severity == "unknown") &&
                 ($siteStatus > 2) ) {
            $categoryContent = $categoryContent . '<li class="unknown"><img class="unknown" height="18pt" src="' . $flag . '" alt="" /><a class="unknown" href="javascript:void(0);" onclick="zoomToSite(window.mapContents[' . $status[$hostName]['site_number'] . ']);">&#128528;' . $site . '</a></li>';
         }
         elseif( ($severity == "ok") &&
                 ($siteStatus == 0) ) {
            $categoryContent = $categoryContent . '<li class="okay"><img class="okay" height="18pt" src="' . $flag . '" alt="" /><a class="okay" href="javascript:void(0);" onclick="zoomToSite(window.mapContents[' . $status[$hostName]['site_number'] . ']);">&#128515;' . $site . '</a></li>';
         }
      }
   }

   if ($categoryContent != "") {
      if ( ($severity == "critical") || ($severity == "warning") ) {
         $addedProblemsSection = true;
      }
      $sitesContent = $sitesContent . "<ul>" . $categoryContent . "</ul>";
   }

   if ($severity == "warning") {
      if ($problems == 0) {
         $sitesContent = $sitesContent . '<p class="allsitesokay" id="sites.noproblems">xxx</p>';
      }
      echo "   document.getElementById(\"sidebar.sites.problems\").innerHTML = '" . $sitesContent . "';\n";
      $sitesContent = "";
   }
   else if ($severity == "unknown") {
      echo "   document.getElementById(\"sidebar.sites.okay\").innerHTML = '" . $sitesContent . "';\n";
   }
}
if($problems == 0) {
   echo "   document.getElementById(\"sites.noproblems\").innerHTML = noProblemLabel[displayLanguage];\n";
}

echo "}\n\n";
?>
