/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

// Solution based on https://medium.com/allenhwkim/mobile-friendly-table-b0cb066dbc0e
// Mobile-Friendly Table

window.tarm_io_set_table_tr_labels = function(selector) {
    const table_element = document.querySelector(selector);
    const th_elements = table_element.querySelectorAll('thead th');
    const td_labels = Array.from(th_elements).map(element => element.innerText);
    table_element.querySelectorAll('tbody tr').forEach(tr => {
        Array.from(tr.children).forEach( 
            (td, ndx) =>  td.setAttribute('label', td_labels[ndx])
        );
    });
}