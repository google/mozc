// Copyright 2010-2013, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package org.mozc.android.inputmethod.japanese.keyboard;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardViewBackgroundSurface.SurfaceCanvas;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;

import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.MoreAsserts;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.easymock.IMocksControl;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;

/**
 */
public class KeyboardViewBackgroundSurfaceTest extends InstrumentationTestCaseWithMock {
  private static final int WIDTH = 50;
  private static final int HEIGHT = 30;
  private static final int HORIZONTAL_GAP = 4;

  private DrawableCache createDrawableCacheMock() {
    return createMockBuilder(DrawableCache.class)
        .withConstructor(MozcDrawableFactory.class)
        .withArgs(new MozcDrawableFactory(new MockResources()))
        .createMock();
  }

  private static Key createDummyKey(int x, int y, int iconResourceId, DrawableType drawableType) {
    KeyEntity keyEntity = new KeyEntity(
        1, 'a', KeyEntity.INVALID_KEY_CODE, iconResourceId, drawableType, false, null);
    Flick centerFlick = new Flick(Direction.CENTER, keyEntity);
    KeyState keyState = new KeyState(EnumSet.of(MetaState.UNMODIFIED), null,
                                     Collections.singletonList(centerFlick));
    return new Key(x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false,
                   Stick.EVEN, Collections.singletonList(keyState));
  }

  @SmallTest
  public void testInitialize() {
    KeyboardViewBackgroundSurface surface = new KeyboardViewBackgroundSurface(null, null);

    // If the surface is an instance constructed right now, initialization is needed later.
    surface.requestUpdateSize(100, 100);
    assertTrue(surface.isInitializationNeeded());

    // We don't need initialization twice consecutively.
    surface.initialize();
    assertFalse(surface.isInitializationNeeded());

    // If given view size is different from the last initialization's, we need to initialize
    // again.
    surface.requestUpdateSize(200, 300);
    assertTrue(surface.isInitializationNeeded());

    // Also, after reset for initialized surface, we need to initialize again as well.
    surface.initialize();
    surface.reset();
    assertTrue(surface.isInitializationNeeded());
  }

  @SmallTest
  public void testGetKeyEntityForRendering() {
    KeyEntity centerEntity =
        new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, 0,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, false, null);
    KeyEntity leftEntity =
        new KeyEntity(2, 'b', KeyEntity.INVALID_KEY_CODE, 0,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, false, null);
    Flick centerFlick = new Flick(Direction.CENTER, centerEntity);
    Flick leftFlick = new Flick(Direction.LEFT, leftEntity);

    Key key = new Key(
        0, 0, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false, Stick.EVEN,
        Collections.singletonList(
            new KeyState(EnumSet.of(MetaState.UNMODIFIED), null,
                         Arrays.asList(centerFlick, leftFlick))));

    // If this is the released key, the default key entity (center's one) should be returned.
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(key, MetaState.UNMODIFIED, null));

    // If this is the pressed key without flick, the center key entity should be returned.
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, MetaState.UNMODIFIED, Direction.CENTER));

    // If the flick state is set to the pressedKey, it should be returned.
    assertSame(
        leftEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, MetaState.UNMODIFIED, Direction.LEFT));

    // If the key doesn't have a KeyEntity for the current state, the default key entity
    // should be returned.
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, MetaState.CAPS_LOCK, Direction.RIGHT));
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(key, MetaState.CAPS_LOCK, null));
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, MetaState.CAPS_LOCK, Direction.CENTER));
  }

  @SmallTest
  public void testGetKeyBackground_withoutBackground() {
    KeyboardViewBackgroundSurface surface =
        new KeyboardViewBackgroundSurface(new BackgroundDrawableFactory(1f), null);

    // Null should be returned for null KeyEntity.
    assertNull(surface.getKeyBackground(null, true));
    assertNull(surface.getKeyBackground(null, false));

    // Null should be returned for a KeyEntity without background.
    KeyEntity keyEntity = new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, 0, null, false, null);
    assertNull(surface.getKeyBackground(keyEntity, true));
    assertNull(surface.getKeyBackground(keyEntity, false));
  }

  @SmallTest
  public void testGetKeyBackground_withBackground() {
    BackgroundDrawableFactory factory = new BackgroundDrawableFactory(1f);

    Drawable background = factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    replayAll();

    KeyEntity keyEntity =
        new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, 0,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, false, null);

    KeyboardViewBackgroundSurface surface = new KeyboardViewBackgroundSurface(factory, null);

    // The icon should be returned with appropriate state.
    assertSame(background, surface.getKeyBackground(keyEntity, false));
    MoreAsserts.assertEquals(new int[]{}, background.getState());

    assertSame(background, surface.getKeyBackground(keyEntity, true));
    MoreAsserts.assertEquals(new int[]{ android.R.attr.state_pressed }, background.getState());
  }

  @SmallTest
  public void testGetKeyIcon_withoutIcon() {
    DrawableCache drawableCache = new DrawableCache(new MozcDrawableFactory(new MockResources()));

    // Null should be returned for null KeyEntity.
    assertNull(KeyboardViewBackgroundSurface.getKeyIcon(drawableCache, null, true));
    assertNull(KeyboardViewBackgroundSurface.getKeyIcon(drawableCache, null, false));

    // Null should be returned for a KeyEntity without icon.
    KeyEntity keyEntity = new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, 0, null, false, null);
    assertNull(KeyboardViewBackgroundSurface.getKeyIcon(drawableCache, keyEntity, true));
    assertNull(KeyboardViewBackgroundSurface.getKeyIcon(drawableCache, keyEntity, false));
  }

  @SmallTest
  public void testGetKeyIcon_withIcon() {
    Drawable icon = new ColorDrawable();
    int iconResourceId = 1;
    DrawableCache drawableCache = createDrawableCacheMock();
    expect(drawableCache.getDrawable(iconResourceId)).andStubReturn(icon);
    replayAll();
    KeyEntity keyEntity =
        new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, iconResourceId, null, false, null);

    // The icon should be returned with appropriate state.
    assertSame(icon, KeyboardViewBackgroundSurface.getKeyIcon(drawableCache, keyEntity, false));
    MoreAsserts.assertEquals(new int[]{}, icon.getState());

    assertSame(icon, KeyboardViewBackgroundSurface.getKeyIcon(drawableCache, keyEntity, true));
    MoreAsserts.assertEquals(new int[]{ android.R.attr.state_pressed }, icon.getState());
  }

  @MediumTest
  public void testSenarioTest() {
    IMocksControl drawableCacheControl = createControl();
    DrawableCache drawableCache = createMockBuilder(DrawableCache.class)
        .withConstructor(MozcDrawableFactory.class)
        .withArgs(new MozcDrawableFactory(new MockResources()))
        .createMock(drawableCacheControl);

    Drawable icon1 = new ColorDrawable();
    int icon1ResourceId = 1;
    Key key1 =
        createDummyKey(0, 0, icon1ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon1ResourceId)).andStubReturn(icon1);

    Drawable icon2 = new ColorDrawable();
    int icon2ResourceId = 3;
    Key key2 =
        createDummyKey(WIDTH, 0, icon2ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon2ResourceId)).andStubReturn(icon2);

    Drawable icon3 = new ColorDrawable();
    int icon3ResourceId = 5;
    Key key3 = createDummyKey(
        WIDTH * 2, 0, icon3ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon3ResourceId)).andStubReturn(icon3);

    Drawable icon4 = new ColorDrawable();
    int icon4ResourceId = 7;
    Key key4 = createDummyKey(
        0, HEIGHT, icon4ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon4ResourceId)).andStubReturn(icon4);

    Drawable icon5 = new ColorDrawable();
    int icon5ResourceId = 9;
    Key key5 = createDummyKey(
        WIDTH, HEIGHT, icon5ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon5ResourceId)).andStubReturn(icon5);

    Drawable icon6 = new ColorDrawable();
    int icon6ResourceId = 11;
    Key key6 = createDummyKey(
        WIDTH * 2, HEIGHT, icon6ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon6ResourceId)).andStubReturn(icon6);

    Drawable icon7 = new ColorDrawable();
    Drawable leftIcon7 = new ColorDrawable();
    int icon7ResourceId = 13;
    int leftIcon7ResourceId = 15;
    Flick centerFlick7 = new Flick(
        Direction.CENTER,
        new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, icon7ResourceId,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, false, null));
    Flick leftFlick7 = new Flick(
        Direction.LEFT,
        new KeyEntity(2, 'a', KeyEntity.INVALID_KEY_CODE, leftIcon7ResourceId,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, false, null));
    KeyState keyState = new KeyState(EnumSet.of(MetaState.UNMODIFIED), null,
                                     Arrays.asList(centerFlick7, leftFlick7));
    Key key7 = new Key(0, HEIGHT * 2, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false,
                       Stick.EVEN, Collections.singletonList(keyState));
    expect(drawableCache.getDrawable(icon7ResourceId)).andStubReturn(icon7);
    expect(drawableCache.getDrawable(leftIcon7ResourceId)).andStubReturn(leftIcon7);

    Drawable icon8 = new ColorDrawable();
    int icon8ResourceId = 17;
    Key key8 = createDummyKey(
        WIDTH, HEIGHT * 2, icon8ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon8ResourceId)).andStubReturn(icon8);

    Drawable icon9 = new ColorDrawable();
    int icon9ResourceId = 19;
    Key key9 = createDummyKey(
        WIDTH * 2, HEIGHT * 2, icon9ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon9ResourceId)).andStubReturn(icon9);
    drawableCacheControl.replay();

    Keyboard keyboard = new Keyboard(
        Arrays.asList(
            new Row(Arrays.asList(key1, key2, key3), HEIGHT, 0),
            new Row(Arrays.asList(key4, key5, key6), HEIGHT, 0),
            new Row(Arrays.asList(key7, key8, key9), HEIGHT, 0)),
        1);

    IMocksControl canvasControl = createControl();
    final SurfaceCanvas canvas = canvasControl.createMock(SurfaceCanvas.class);
    int surfaceWidth = WIDTH * 3;
    int surfaceHeight = HEIGHT * 3;
    int expectedKeyWidth = WIDTH - HORIZONTAL_GAP;
    int expectedKeyHeight = HEIGHT;

    BackgroundDrawableFactory factory = new BackgroundDrawableFactory(1f);
    Drawable background = factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);

    // The first case is just a simple senario, just setting a new keyboard.
    canvas.clearRegion(0, 0, surfaceWidth, surfaceHeight);
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    KeyboardViewBackgroundSurface surface =
        new KeyboardViewBackgroundSurface(factory, drawableCache) {
      @Override
      void initialize() {
        super.initialize();
        this.surfaceCanvas = canvas;
      }
    };

    surface.requestUpdateSize(surfaceWidth, surfaceHeight);
    surface.requestUpdateKeyboard(keyboard, MetaState.UNMODIFIED);
    surface.update();

    canvasControl.verify();

    // The second case is just updating some keys.
    canvasControl.reset();
    canvas.clearRegion(WIDTH, HEIGHT, WIDTH, HEIGHT);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.clearRegion(0, HEIGHT * 2, WIDTH, HEIGHT);
    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        leftIcon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    surface.requestUpdateKey(key5, Direction.CENTER);
    surface.requestUpdateKey(key7, Direction.LEFT);
    surface.update();

    canvasControl.verify();

    // The third senario is updating some keys multiple times.
    canvasControl.reset();
    canvas.clearRegion(WIDTH, HEIGHT * 2, WIDTH, HEIGHT);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);

    canvas.clearRegion(0, 0, WIDTH, HEIGHT);
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvasControl.replay();

    surface.requestUpdateKey(key8, Direction.CENTER);
    surface.requestUpdateKey(key8, Direction.LEFT);
    surface.requestUpdateKey(key8, Direction.RIGHT);
    surface.requestUpdateKey(key8, Direction.CENTER);
    surface.requestUpdateKey(key8, Direction.UP);
    surface.requestUpdateKey(key8, Direction.DOWN);
    surface.requestUpdateKey(key8, null);

    surface.requestUpdateKey(key1, Direction.CENTER);
    surface.requestUpdateKey(key1, Direction.UP);
    surface.requestUpdateKey(key1, Direction.DOWN);
    surface.requestUpdateKey(key1, Direction.CENTER);

    surface.update();

    canvasControl.verify();

    // The fourth senario is updating some keys, and then setting the keyboard.
    canvasControl.reset();
    canvas.clearRegion(0, 0, surfaceWidth, surfaceHeight);
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    surface.requestUpdateKey(key5, Direction.CENTER);
    surface.requestUpdateKey(key7, Direction.LEFT);

    surface.requestUpdateKeyboard(keyboard, MetaState.UNMODIFIED);
    surface.update();

    canvasControl.verify();

    // The fifth senario is updating some keys, setting the keyboard, and then updating some keys.
    canvasControl.reset();
    canvas.clearRegion(0, 0, surfaceWidth, surfaceHeight);
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        leftIcon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    surface.requestUpdateKey(key5, Direction.CENTER);
    surface.requestUpdateKey(key2, Direction.LEFT);

    surface.requestUpdateKeyboard(keyboard, MetaState.UNMODIFIED);

    surface.requestUpdateKey(key8, Direction.CENTER);
    surface.requestUpdateKey(key8, Direction.LEFT);
    surface.requestUpdateKey(key8, Direction.RIGHT);
    surface.requestUpdateKey(key8, Direction.CENTER);
    surface.requestUpdateKey(key8, Direction.UP);
    surface.requestUpdateKey(key8, Direction.DOWN);
    surface.requestUpdateKey(key8, null);

    surface.requestUpdateKey(key1, Direction.CENTER);
    surface.requestUpdateKey(key1, Direction.UP);
    surface.requestUpdateKey(key1, Direction.DOWN);
    surface.requestUpdateKey(key1, Direction.CENTER);

    surface.requestUpdateKey(key7, Direction.LEFT);

    surface.update();

    canvasControl.verify();
  }
}
