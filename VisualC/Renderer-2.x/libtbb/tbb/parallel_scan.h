/*
    Copyright 2005-2009 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#ifndef __TBB_parallel_scan_H
#define __TBB_parallel_scan_H

#include "task.h"
#include "aligned_space.h"
#include <new>
#include "partitioner.h"

namespace tbb {

//! Used to indicate that the initial scan is being performed.
/** @ingroup algorithms */
struct pre_scan_tag {
    static bool is_final_scan() {return false;}
};

//! Used to indicate that the final scan is being performed.
/** @ingroup algorithms */
struct final_scan_tag {
    static bool is_final_scan() {return true;}
};

//! @cond INTERNAL
namespace internal {

    //! Performs final scan for a leaf 
    /** @ingroup algorithms */
    template<typename Range, typename Body>
    class final_sum: public task {
    public:
        Body body;
    private:
        aligned_space<Range,1> range;
        //! Where to put result of last subrange, or NULL if not last subrange.
        Body* stuff_last;
    public:
        final_sum( Body& body_ ) :
            body(body_,split())
        {
            poison_pointer(stuff_last);
        }
        ~final_sum() {
            range.begin()->~Range();
        }     
        void finish_construction( const Range& range_, Body* stuff_last_ ) {
            new( range.begin() ) Range(range_);
            stuff_last = stuff_last_;
        }
    private:
        /*override*/ task* execute() {
            body( *range.begin(), final_scan_tag() );
            if( stuff_last )
                stuff_last->assign(body);
            return NULL;
        }
    };       

    //! Split work to be done in the scan.
    /** @ingroup algorithms */
    template<typename Range, typename Body>
    class sum_node: public task {
        typedef final_sum<Range,Body> final_sum_type;
    public:
        final_sum_type *incoming; 
        final_sum_type *body;
        Body *stuff_last;
    private:
        final_sum_type *left_sum;
        sum_node *left;
        sum_node *right;     
        bool left_is_final;
        Range range;
        sum_node( const Range range_, bool left_is_final_ ) : 
            left_sum(NULL), 
            left(NULL), 
            right(NULL), 
            left_is_final(left_is_final_), 
            range(range_)
        {
            // Poison fields that will be set by second pass.
            poison_pointer(body);
            poison_pointer(incoming);
        }
        task* create_child( const Range& range, final_sum_type& f, sum_node* n, final_sum_type* incoming, Body* stuff_last ) {
            if( !n ) {
                f.recycle_as_child_of( *this );
                f.finish_construction( range, stuff_last );
                return &f;
            } else {
                n->body = &f;
                n->incoming = incoming;
                n->stuff_last = stuff_last;
                return n;
            }
        }
        /*override*/ task* execute() {
            if( body ) {
                if( incoming )
                    left_sum->body.reverse_join( incoming->body );
                recycle_as_continuation();
                sum_node& c = *this;
                task* b = c.create_child(Range(range,split()),*left_sum,right,left_sum,stuff_last);
                task* a = left_is_final ? NULL : c.create_child(range,*body,left,incoming,NULL);
                set_ref_count( (a!=NULL)+(b!=NULL) );
                body = NULL; 
                if( a ) spawn(*b);
                else a = b;
                return a;
            } else {
                return NULL;
            }
        }
        template<typename Range_,typename Body_,typename Partitioner_>
        friend class start_scan;

        template<typename Range_,typename Body_>
        friend class finish_scan;
    };

    //! Combine partial results
    /** @ingroup algorithms */
    template<typename Range, typename Body>
    class finish_scan: public task {
        typedef sum_node<Range,Body> sum_node_type;
        typedef final_sum<Range,Body> final_sum_type;
        final_sum_type** const sum;
        sum_node_type*& return_slot;
    public:
        final_sum_type* right_zombie;
        sum_node_type& result;

        /*override*/ task* execute() {
            __TBB_ASSERT( result.ref_count()==(result.left!=NULL)+(result.right!=NULL), NULL );
            if( result.left )
                result.left_is_final = false;
            if( right_zombie && sum ) 
                ((*sum)->body).reverse_join(result.left_sum->body);
            __TBB_ASSERT( !return_slot, NULL );
            if( right_zombie || result.right ) {
                return_slot = &result;
            } else {
                destroy( result );
            }
            if( right_zombie && !sum && !result.right ) {
                destroy(*right_zombie);
                right_zombie = NULL;
            }
            return NULL;
        }

        finish_scan( sum_node_type*& return_slot_, final_sum_type** sum_, sum_node_type& result_ ) : 
            sum(sum_),
            return_slot(return_slot_), 
            right_zombie(NULL),
            result(result_)
        {
            __TBB_ASSERT( !return_slot, NULL );
        }
        ~finish_scan(){
#if __TBB_EXCEPTIONS
            if (is_cancelled()) {
                if (result.ref_count() == 0) destroy(result);
                if (right_zombie) destroy(*right_zombie);
            }
#endif
        }
    };

    //! Initial task to split the work
    /** @ingroup algorithms */
    template<typename Range, typename Body, typename Partitioner=simple_partitioner>
    class start_scan: public task {
        typedef sum_node<Range,Body> sum_node_type;
        typedef final_sum<Range,Body> final_sum_type;
        final_sum_type* body;
        /** Non-null if caller is requesting total. */
        final_sum_type** sum; 
        sum_node_type** return_slot;
        /** Null if computing root. */
        sum_node_type* parent_sum;
        bool is_final;
        bool is_right_child;
        Range range;
        typename Partitioner::partition_type partition;
        /*override*/ task* execute();
#if __TBB_EXCEPTIONS
        tbb::task_group_context &my_context;
#endif
    public:
        start_scan( sum_node_type*& return_slot_, start_scan& parent, sum_node_type* parent_sum_ 
#if __TBB_EXCEPTIONS
            , tbb::task_group_context &_context
#endif
            ) :
            body(parent.body),
            sum(parent.sum),
            return_slot(&return_slot_),
            parent_sum(parent_sum_),
            is_final(parent.is_final),
            is_right_child(false),
            range(parent.range,split()),
            partition(parent.partition,split())
#if __TBB_EXCEPTIONS
        , my_context (_context)
#endif
        {
            __TBB_ASSERT( !*return_slot, NULL );
        }

        start_scan( sum_node_type*& return_slot_, const Range& range_, final_sum_type& body_, const Partitioner& partitioner_
#if __TBB_EXCEPTIONS
        , tbb::task_group_context &_context
#endif
            ) :
            body(&body_),
            sum(NULL),
            return_slot(&return_slot_),
            parent_sum(NULL),
            is_final(true),
            is_right_child(false),
            range(range_),
            partition(partitioner_)
#if __TBB_EXCEPTIONS
            , my_context (_context)
#endif
        {
            __TBB_ASSERT( !*return_slot, NULL );
        }

        static void run(  const Range& range, Body& body, const Partitioner& partitioner 
#if __TBB_EXCEPTIONS
        , task_group_context& context
#endif
            ) {
            if( !range.empty() ) {
                typedef internal::start_scan<Range,Body,Partitioner> start_pass1_type;
                internal::sum_node<Range,Body>* root = NULL;
                typedef internal::final_sum<Range,Body> final_sum_type;
#if __TBB_EXCEPTIONS
                final_sum_type* temp_body = new(task::allocate_root(context)) final_sum_type( body );
                start_pass1_type& pass1 = *new(task::allocate_root(context)) start_pass1_type(
                    /*return_slot=*/root,
                    range,
                    *temp_body,
                    partitioner,
                    context
                    );
#else
                final_sum_type* temp_body = new(task::allocate_root()) final_sum_type( body );
                start_pass1_type& pass1 = *new(task::allocate_root()) start_pass1_type(
                    /*return_slot=*/root,
                    range,
                    *temp_body,
                    partitioner );
#endif
                // The class is intended to destroy allocated tasks if exception occurs
                class task_cleaner: internal::no_copy {
                    internal::sum_node<Range,Body>* my_root;
                    final_sum_type* my_temp_body;
                    const Range& my_range;
                    Body& my_body;
                    start_pass1_type* my_pass1;
                public:
                    bool do_clean; // Set to true if cleanup is required.
                    task_cleaner(internal::sum_node<Range,Body>* _root, final_sum_type* _temp_body, const Range& _range, Body& _body, start_pass1_type* _pass1)
                        : my_root(_root), my_temp_body(_temp_body), my_range(_range), my_body(_body), my_pass1(_pass1), do_clean(true) {}
                    ~task_cleaner(){
                        if (do_clean) {
                            my_body.assign(my_temp_body->body);
                            my_temp_body->finish_construction( my_range, NULL );
                            my_temp_body->destroy(*my_temp_body);
                        }
                    }
                };
                task_cleaner my_cleaner(root, temp_body, range, body, &pass1);

                task::spawn_root_and_wait( pass1 );
                my_cleaner.do_clean = false;
                if( root ) {
                    root->body = temp_body;
                    root->incoming = NULL;
                    root->stuff_last = &body;
                    task::spawn_root_and_wait( *root );
                } else {
                    my_cleaner.do_clean = true;
                }
            }
        }
    };

    template<typename Range, typename Body, typename Partitioner>
    task* start_scan<Range,Body,Partitioner>::execute() {
        typedef internal::finish_scan<Range,Body> finish_pass1_type;
        finish_pass1_type* p = parent_sum ? static_cast<finish_pass1_type*>( parent() ) : NULL;
        // Inspecting p->result.left_sum would ordinarily be a race condition.
        // But we inspect it only if we are not a stolen task, in which case we
        // know that task assigning to p->result.left_sum has completed.
        bool treat_as_stolen = is_right_child && (is_stolen_task() || body!=p->result.left_sum);
        if( treat_as_stolen ) {
            // Invocation is for right child that has been really stolen or needs to be virtually stolen
#if __TBB_EXCEPTIONS
            p->right_zombie = body = new( allocate_root(my_context) ) final_sum_type(body->body);
#else
            p->right_zombie = body = new( allocate_root() ) final_sum_type(body->body);
#endif
            is_final = false;
        }
        task* next_task = NULL;
        if( (is_right_child && !treat_as_stolen) || !range.is_divisible() || partition.should_execute_range(*this) ) {
            if( is_final )
                (body->body)( range, final_scan_tag() );
            else if( sum )
                (body->body)( range, pre_scan_tag() );
            if( sum ) 
                *sum = body;
            __TBB_ASSERT( !*return_slot, NULL );
        } else {
            sum_node_type* result;
            if( parent_sum ) 
                result = new(allocate_additional_child_of(*parent_sum)) sum_node_type(range,/*left_is_final=*/is_final);
            else
#if __TBB_EXCEPTIONS
                result = new(task::allocate_root(my_context)) sum_node_type(range,/*left_is_final=*/is_final);
#else
                result = new(task::allocate_root()) sum_node_type(range,/*left_is_final=*/is_final);
#endif
            finish_pass1_type& c = *new( allocate_continuation()) finish_pass1_type(*return_slot,sum,*result);
            // Split off right child
#if __TBB_EXCEPTIONS
            start_scan& b = *new( c.allocate_child() ) start_scan( /*return_slot=*/result->right, *this, result, my_context );
#else
            start_scan& b = *new( c.allocate_child() ) start_scan( /*return_slot=*/result->right, *this, result );
#endif
            b.is_right_child = true;    
            // Left child is recycling of *this.  Must recycle this before spawning b, 
            // otherwise b might complete and decrement c.ref_count() to zero, which
            // would cause c.execute() to run prematurely.
            recycle_as_child_of(c);
            c.set_ref_count(2);
            c.spawn(b);
            sum = &result->left_sum;
            return_slot = &result->left;
            is_right_child = false;
            next_task = this;
            parent_sum = result; 
            __TBB_ASSERT( !*return_slot, NULL );
        }
        return next_task;
    } 
} // namespace internal
//! @endcond

// Requirements on Range concept are documented in blocked_range.h

/** \page parallel_scan_body_req Requirements on parallel_scan body
    Class \c Body implementing the concept of parallel_reduce body must define:
    - \code Body::Body( Body&, split ); \endcode    Splitting constructor.
                                                    Split \c b so that \c this and \c b can accumulate separately
    - \code Body::~Body(); \endcode                 Destructor
    - \code void Body::operator()( const Range& r, pre_scan_tag ); \endcode
                                                    Preprocess iterations for range \c r
    - \code void Body::operator()( const Range& r, final_scan_tag ); \endcode 
                                                    Do final processing for iterations of range \c r
    - \code void Body::reverse_join( Body& a ); \endcode
                                                    Merge preprocessing state of \c a into \c this, where \c a was 
                                                    created earlier from \c b by b's splitting constructor
**/

/** \name parallel_scan
    See also requirements on \ref range_req "Range" and \ref parallel_scan_body_req "parallel_scan Body". **/
//@{

//! Parallel prefix with default partitioner
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_scan( const Range& range, Body& body ) {
#if __TBB_EXCEPTIONS
    task_group_context context;
#endif // __TBB_EXCEPTIONS
    internal::start_scan<Range,Body,__TBB_DEFAULT_PARTITIONER>::run(range,body,__TBB_DEFAULT_PARTITIONER()
#if __TBB_EXCEPTIONS
        , context
#endif
        );
}

//! Parallel prefix with simple_partitioner
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_scan( const Range& range, Body& body, const simple_partitioner& partitioner ) {
#if __TBB_EXCEPTIONS
    task_group_context context;
#endif // __TBB_EXCEPTIONS
    internal::start_scan<Range,Body,simple_partitioner>::run(range,body,partitioner
#if __TBB_EXCEPTIONS
        , context
#endif
        );
}

//! Parallel prefix with auto_partitioner
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_scan( const Range& range, Body& body, const auto_partitioner& partitioner ) {
#if __TBB_EXCEPTIONS
    task_group_context context;
#endif // __TBB_EXCEPTIONS
    internal::start_scan<Range,Body,auto_partitioner>::run(range,body,partitioner
#if __TBB_EXCEPTIONS
        , context
#endif
        );
}
#if __TBB_EXCEPTIONS
//! Parallel prefix with simple_partitioner and user-supplied context
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_scan( const Range& range, Body& body, const simple_partitioner& partitioner, tbb::task_group_context & context ) {
    internal::start_scan<Range,Body,simple_partitioner>::run(range,body,partitioner,context);
}

//! Parallel prefix with auto_partitioner and user-supplied context
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_scan( const Range& range, Body& body, const auto_partitioner& partitioner, tbb::task_group_context & context ) {
    internal::start_scan<Range,Body,auto_partitioner>::run(range,body,partitioner,context);
}

//! Parallel prefix with default partitioner and user-supplied context
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_scan( const Range& range, Body& body, tbb::task_group_context & context ) {
    internal::start_scan<Range,Body,__TBB_DEFAULT_PARTITIONER>::run(range,body,__TBB_DEFAULT_PARTITIONER(),context);
}
#endif

//@}

} // namespace tbb

#endif /* __TBB_parallel_scan_H */

