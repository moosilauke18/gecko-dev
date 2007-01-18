# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Table - Produces display tables for Testopia lists

=head1 DESCRIPTION

A table is generated as a result of a query. This module returns 
a list of Testopia objects that were queried for. It supports 
pagination of data as well as sorting. It takes the following 
arguments:

=over 

=item type - one of 'case', 'plan', 'run', 'caserun', or 'environment'

=item url - the cgi file that is calling this 

=item cgi - a CGI object

=item list - A reference to a list

=item query - An SQL query string, usually generated by Search.pm

=back

=head1 SYNOPSIS

 $table = Bugzilla::Testopia::Table->new($type, $url, $cgi, $list, $query);

=cut

package Bugzilla::Testopia::Table;

use strict;

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::TestCaseRun;

###############################
####    Initialization     ####
###############################

# For use in sorting functions which do not allow arguments
our $field;
our $reverse;

=head1 METHODS

=head2 new

Instantiates a new table object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    my $self = {};
    bless($self, $class);

    $self->init(@_);
 
    return $self;
}

=head2 init

Private constructor for this class

=cut

sub init {
    my $self = shift;
    my ($type, $url, $cgi, $list, $query)  = @_;
    $self->{'user'} = Bugzilla->user;
    $self->{'type'} = $type ||   ThrowCodeError('bad_arg',
                                   {argument => 'type',
                                    function => 'Testopia::Table::_init'});
    $self->{'url_loc'} = $url;
    $self->{'cgi'} = $cgi;
    my $debug = $cgi->param('debug') if $cgi;
    my @list;
    if ($query){
        my $serverpush = support_server_push($cgi);
        print "$query" if $debug;
        # For paging we need to know the total number of items
        # but Search.pm returns a query with a subset
        my $countquery = $query;
        $countquery =~ s/ LIMIT.*$//;
        print "<br> $countquery" if $debug;
        my $dbh = Bugzilla->switch_to_shadow_db();
        my $count_res = $dbh->selectcol_arrayref($countquery);
        my $count = scalar @$count_res;
        print "<br> total rows: $count" if $debug;
        $self->{'list_count'} = $count;
        my @ids;
        my $list = $dbh->selectcol_arrayref($query);
        $dbh = Bugzilla->switch_to_main_db();
        
        my $vars;
        my $progress_interval = 1000;
        my $i = 0;
        my $total = scalar @$list;
        
        foreach my $id (@$list){
            $i++;
            if ($serverpush && $i % $progress_interval == 0){
                print $cgi->multipart_end;
                print $cgi->multipart_start;
                $vars->{'complete'} = $i;
                $vars->{'total'} = $total;
                
                Bugzilla->template->process("testopia/progress.html.tmpl", $vars)
                  || ThrowTemplateError(Bugzilla->template->error());
            }
            my $o;
            if ($type eq 'case'){
                $o = Bugzilla::Testopia::TestCase->new($id);
            }
            elsif ($type eq 'plan'){
                $o = Bugzilla::Testopia::TestPlan->new($id);
            }
            elsif ($type eq 'run'){
                $o = Bugzilla::Testopia::TestRun->new($id);
            }
            elsif ($type eq 'case_run'){
                $o = Bugzilla::Testopia::TestCaseRun->new($id);
            }
            elsif ($type eq 'environment'){
                $o = Bugzilla::Testopia::Environment->new($id);
            }
            push (@ids, $id);
            push (@list, $o);
        }
        $self->{'list'} = \@list;
        $self->{'view_count'} = scalar @list; 
        $self->{'id_list'} = join(",", @$count_res);
    }
    if ($cgi){
        $self->{'viewall'} = $cgi->param('viewall');
        $self->{'page'} = $cgi->param('page') || 0;
    }
            
#    elsif (!$query && !$list){
#        my @list;
#        foreach my $id (split(",", $self->get_saved_list())){
#            if ($self->{'type'} eq 'case'){
#                my $o = Bugzilla::Testopia::TestCase->new($id);
#                push @list, $o;
#                $o->category;
#                $o->status;
#                $o->priority;
#            }
#            elsif ($self->{'type'} eq 'plan'){
#                my $o = Bugzilla::Testopia::TestPlan->new($id);
#                push @list, $o;
#                $o->test_case_count;
#                $o->test_run_count;
#            }
#            elsif ($self->{'type'} eq 'run'){
#                my $o = Bugzilla::Testopia::TestCase->new($id);
#                push @list, $o;
##                $o->category;
##                $o->status;
##                $o->priority;                
#            }
#            elsif ($self->{'type'} eq 'caserun'){
#                my $o = Bugzilla::Testopia::TestCase->new($id);
#                push @list, $o;
##                $o->category;
##                $o->status;
##                $o->priority;
#            }
#            elsif ($self->{'type'} eq 'attachment'){
#                my $o = Bugzilla::Testopia::TestCase->new($id);
#                push @list, $o;
##                $o->category;
##                $o->status;
##                $o->priority;
#            }
#            else {
#                ThrowUserError('unknown-type');
#            }
#        }
#        $self->{'list'} = \@list;
#    }
#    else {
#        $self->{'list'} = $list;
#    }
#    my @params = split(":", $cgi->cookie('TESTORDER'));
#    $self->{'last_sort'} = shift @params || undef;
#    $self->{'reverse_sort'} = shift @params || undef;
#
#    #### SORT ####
#    # This is very inefficient. It would be much better to have 
#    # the database do this.
#    my $order = $cgi->param('order');
#    if ($order){
#        $self->sort_fields($order);
#        $self->{'reverse_sort'} = ($self->{'reverse_sort'} ? 1 : 0);
#    }
#    
    #### SAVE ####
    # Save the list of testcases for use in paginating and sorting
    $self->save_list;
#
#    #### SPLICE ####
#    # If we are using a paged view of the data we split it up here 
#    $self->get_page($self->{'page'});
    
}

###############################
####       Methods         ####
###############################

=head2 save_list

Saves the last list to the database as a hidden saved search
Used only in list context

=cut

sub save_list {
    my $self = shift;
    return if ($self->{'user'}->id == 0);
#    my @ids;
#    foreach my $i (@{$self->{'list'}}){
#            push @ids, $i->id;
#    } 
#    my $list = join(",", @ids);
    my $dbh = Bugzilla->dbh;
    if ($self->{'id_list'}){
        $dbh->bz_lock_tables('test_named_queries WRITE');
        my ($is) = $dbh->selectrow_array(
                "SELECT 1 FROM test_named_queries 
                 WHERE userid = ? AND name = ?", undef,
                 ($self->{'user'}->id, "__". $self->{'type'} ."__"));
    
        if ($is) {
            $dbh->do("UPDATE test_named_queries SET query = ?
                      WHERE userid = ? AND name = ?", undef,
                      (join(",", $self->{'id_list'}), $self->{'user'}->id, "__". $self->{'type'} ."__"));
        }
        else {
            $dbh->do("INSERT INTO test_named_queries (userid, name, isvisible, query) VALUES(?,?,?,?)", undef,
                      ($self->{'user'}->id, "__". $self->{'type'} ."__", 0, join(",", $self->{'id_list'})));
        }
        $dbh->bz_unlock_tables();
    }
#    $self->{'list_count'} = scalar @ids unless $self->{'query'};
}

=head2 get_saved_list

Retrieves a saved list from the database
Used only in list context

=cut

sub get_saved_list {
    my $self = shift;
    return undef if ($self->{'user'}->id == 0);
    my $type = shift || $self->{'type'};
    my $dbh = Bugzilla->dbh;
    my ($list) = $dbh->selectrow_array(
            "SELECT query FROM test_named_queries 
             WHERE userid = ? AND name = ?", undef,
             ($self->{'user'}->id, "__". $type ."__"));
    my @list = split(',', $list);
    return \@list;
}

# used by the sort function to check which field to sort on
# we turn off warnings to supress the nonnumeric vs numeric junk
sub sort_fields {
    no warnings;
    if ($field eq 'category'){
        if ($reverse){
            $a->category->name cmp $b->category->name;
        }
        else {    
            $b->category->name cmp $a->category->name; 
        }
    }
    elsif ($field eq 'plans'){
        if ($reverse){
            scalar @{$a->plans} <=> scalar @{$b->plans};
        }
        else {    
            scalar @{$b->plans} <=> scalar @{$a->plans};
        }
    }
    else{
        if ($reverse){
            $a->{$field} cmp $b->{$field};
        }
        else {    
            $b->{$field} cmp $a->{$field}; 
        }
    }
    use warnings;
}

sub sort_list {
    my $self = shift;
    $field = shift;
    $reverse = $self->{'reverse_sort'};
    my @list = sort sort_fields @{$self->{'list'}};
    $self->{'list'} = \@list; 
    return $self->{'list'};
}

sub get_page {
    my $self = shift; 
    if ($self->{'viewall'}){
        return $self->{'list'};
    }
    my $pagenum = shift || $self->{'page'};
    $self->{'page'} = $pagenum;
    my $offset = $pagenum * $self->page_size;
    my @list = @{$self->{'list'}};
    @list = splice(@list, $offset, $self->page_size);
    $self->{'list'} = \@list;
    return $self->{'list'};
}

sub get_next{
    my $self = shift; 
    my ($curr) = @_;
    
    my $list = $self->get_list;
    my $ref = lsearch($curr, $list);
    return undef if $ref == -1;
    return $list->[$ref];
}
###############################
####      Accessors        ####
###############################

sub list       { return $_[0]->{'list'};       }
sub id_list    { return $_[0]->{'id_list'};       }
sub list_count { return $_[0]->{'list_count'}; }
sub view_count { return $_[0]->{'view_count'}; }
sub page       { return $_[0]->{'page'}; }
sub url_loc    { return $_[0]->{'url_loc'}; }
sub type       { return $_[0]->{'type'}; }

=head2 page_size

Returns an ineger representing how many items should appear on a page

=cut

sub page_size    {
    my $self = shift;
    my $cgi = $self->{'cgi'};
    return $cgi->param('pagesize') if $cgi->param('pagesize');
    return 25;  #TODO: make this a user setting
}

=head2 get_order_url

Returns a URL query string from a CGI object which is used by 
column headers to produce a sort order

=cut

sub get_order_url {
    my $self = shift;
    return $self->get_url('(page|order)');
}

=head2 get_page_url

Retrns a URL query string from a CGI object which is used by
the page navigation links to move from page to page.

=cut

sub get_page_url {
    my $self = shift;
    return $self->get_url('page');
}

sub get_url {
    my $self = shift;
    my $regxp = shift;
    my $cgi = $self->{'cgi'};
    my @keys = $cgi->param;
    my $qstring;
    foreach my $key (@keys){
        if ($key =~ /$regxp/){
            next;
        }
        my @vals = $cgi->param($key);
        foreach my $val (@vals){
            $qstring .= $key ."=". $val ."&";
        }
    }
    chop $qstring;
    $qstring = $self->{'url_loc'} ."?". $qstring;
    $self->{'url'} = $qstring;
    return $self->{'url'};
}

sub get_query_part {
    my $self = shift;
    my $cgi = $self->{'cgi'};
    my @keys = $cgi->param;
    my $qstring;
    foreach my $key (@keys){
        my @vals = $cgi->param($key);
        foreach my $val (@vals){
            $qstring .= $key ."=". $val ."&";
        }
    }
    chop $qstring;
    return $qstring
}

=head2 page_count

Returns a total count of the number of pages returned by a query.
Determined in part by page_size

=cut

sub page_count {
    my $self = shift;
    if ($self->list_count % $self->page_size){
        use integer;
        return ($self->list_count / $self->page_size) + 1;
    }
    return $self->list_count/$self->page_size;
}

sub reverse_sort {
    my $self = shift;
    return $self->{'reverse_sort'};
}

sub ajax {
    my $self = shift;
    return $self->{'ajax'} ? 1 : 0;
}

sub arrow {
    my $self = shift;
    if ($self->{'reverse_sort'}) {
        $self->{'arrow'} = '<img src="images/arrow_desc.png" border="0">';
    }
    else {
        $self->{'arrow'} = '<img src="images/arrow_asc.png" border="0">';
    }
    return $self->{'arrow'};
}

=head1 SEE ALSO

Testopia::Search

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
